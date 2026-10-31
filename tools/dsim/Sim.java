// Headless runner for the Digital CPU circuit (uses Digital.jar as a library).
//
//   javac -cp Digital.jar -d build Sim.java
//   java -cp "Digital.jar;build" Sim <circuit.dig> <program.bin> <keys> [maxHalfCycles]
//
// keys     : one key is fed each time the program prints "> " (\n allowed)
// options (-Dname=value, before Sim) :
//   dip=x,y;x,y    DipSwitches turned on before the model is built
//                  (default -560,-1640 = clock mux select -> Clock; "dip=" turns none on).
//                  A position without a DipSwitch is an error.
//   set=E=1,run=1  top-level inputs
//   at=H:k=v,...;H:k=v  change top-level inputs at half cycle H (e.g. at=0:E=0,boot=1;10:E=1,boot=0)
//   csv=file       dump every model signal (probes, In/Out) each half cycle
//   csvFrom=N      start the dump at half cycle N (default 0)
//   csvKey=N       or start the dump when the N-th key (1-based) is fed
//   banner=text    text printed once at reset; seeing it again = restart (default none = no restart check)
//   stopRestart=1  stop shortly after a restart is detected (default 1)
//   stall=N        stop when PC, com1 and com2 (stallCols=a,b,...) keep their value N halves (default 2000, 0 = off)
//   idle=N         stop after N halves without terminal output (default 0 = off)
//   patch=name     in-memory circuit patch from Patch.java (e.g. scsfd)
//   probe=f.dig@x,y=name;...  extra Probe on an unnamed net point (f = top or a subcircuit file)
//   cols=a,b,...   with csv : dump only these signals
//   zseed=N|zero   floating (high-Z) bits normally read as fresh random bits on every read (new Random() per
//                  value, so every run differs). N : seeded per value in node order, zero : always 0
//   delta=file     like csv but only changes: header "names...", then "half col value" per changed signal
//   hash=file      write one hash of all signals per half cycle ("half hash"), to find where two runs diverge
//   lines=1       keys is a '|' separated list of lines; one whole line + \n is fed per prompt
import de.neemann.digital.cli.CircuitLoader;
import de.neemann.digital.core.*;
import de.neemann.digital.core.element.Keys;
import de.neemann.digital.core.wiring.Clock;
import de.neemann.digital.draw.elements.VisualElement;
import de.neemann.digital.gui.ProgramMemoryLoader;
import de.neemann.digital.gui.components.terminal.*;

import java.io.*;
import java.util.*;

public class Sim {
    public static void main(String[] a) throws Exception {
        String dig = a[0], prog = a[1], keys = a[2].replace("\\n", "\n");
        boolean lines = System.getProperty("lines") != null;
        String[] items = keys.isEmpty() ? new String[0] : lines ? keys.split("\\|") : keys.split("");
        long maxHalf = a.length > 3 ? Long.parseLong(a[3]) : 2_000_000;

        CircuitLoader cl = new CircuitLoader(new File(dig));
        Set<String> dips = new HashSet<>(Arrays.asList(System.getProperty("dip", "-560,-1640").split(";")));
        dips.remove("");
        for (VisualElement ve : cl.getCircuit().getElements())
            if (ve.getElementName().equals("DipSwitch") && dips.remove(ve.getPos().x + "," + ve.getPos().y))
                ve.getElementAttributes().set(Keys.DIP_DEFAULT, true);
        // a moved DipSwitch would leave the clock unconnected and the run would silently do nothing
        if (!dips.isEmpty()) throw new IllegalArgumentException("no DipSwitch at " + dips + " (-Ddip=x,y;...)");

        if (System.getProperty("patch") != null) Patch.apply(cl, System.getProperty("patch"));

        // tap=top;load-buffer-debug.dig;...  : add a Probe on every Tunnel net of those circuits
        // (top = the main circuit). Probe label = T_<net> (top) or <file>/<net>.
        String tap = System.getProperty("tap");
        if (tap != null)
            for (String which : tap.split(";")) {
                de.neemann.digital.draw.elements.Circuit ci;
                String prefix;
                if (which.equals("top")) { ci = cl.getCircuit(); prefix = "T_"; }
                else {
                    ci = ((de.neemann.digital.draw.library.ElementTypeDescriptionCustom)
                            cl.getLibrary().getElementType(which)).getCircuit();
                    prefix = which.replace(".dig", "") + "/";
                }
                Set<String> seen = new HashSet<>();
                List<VisualElement> add = new ArrayList<>();
                for (VisualElement ve : ci.getElements())
                    if (ve.getElementName().equals("Tunnel")) {
                        String n = ve.getElementAttributes().get(Keys.NETNAME);
                        if (seen.add(n))
                            add.add(new VisualElement("Probe").setPos(ve.getPos())
                                    .setAttribute(Keys.LABEL, prefix + n).setShapeFactory(cl.getShapeFactory()));
                    }
                for (VisualElement ve : add) ci.add(ve);
            }

        // probe=file.dig@x,y=label;top@x,y=label  : a Probe at a pin/wire point (for unnamed nets)
        String probe = System.getProperty("probe");
        if (probe != null)
            for (String p : probe.split(";")) {
                String[] fl = p.split("=");
                String[] wp = fl[0].split("@");
                String[] xy = wp[1].split(",");
                de.neemann.digital.draw.elements.Circuit ci = wp[0].equals("top") ? cl.getCircuit()
                        : ((de.neemann.digital.draw.library.ElementTypeDescriptionCustom)
                        cl.getLibrary().getElementType(wp[0])).getCircuit();
                ci.add(new VisualElement("Probe").setPos(new de.neemann.digital.draw.graphics.Vector(
                                Integer.parseInt(xy[0]), Integer.parseInt(xy[1])))
                        .setAttribute(Keys.LABEL, fl[1]).setShapeFactory(cl.getShapeFactory()));
            }

        // pinprobe=f.dig@Elem@x,y.pin=label;...  : Probe exactly on an element pin (safe for unnamed nets)
        String pinprobe = System.getProperty("pinprobe");
        if (pinprobe != null)
            for (String p : pinprobe.split(";")) {
                String[] fl = p.split("=");
                String[] parts = fl[0].split("@");          // file, Elem, "x,y.pin"
                String[] xyPin = parts[2].split("\\.");
                String[] xy = xyPin[0].split(",");
                de.neemann.digital.draw.elements.Circuit ci = parts[0].equals("top") ? cl.getCircuit()
                        : ((de.neemann.digital.draw.library.ElementTypeDescriptionCustom)
                        cl.getLibrary().getElementType(parts[0])).getCircuit();
                de.neemann.digital.draw.graphics.Vector at = null;
                for (VisualElement ve : ci.getElements())
                    if (ve.getElementName().equals(parts[1]) && ve.getPos().x == Integer.parseInt(xy[0])
                            && ve.getPos().y == Integer.parseInt(xy[1]))
                        for (de.neemann.digital.draw.elements.Pin pn : ve.getPins())
                            if (pn.getName().equals(xyPin[1])) at = pn.getPos();
                if (at == null) throw new IllegalArgumentException("pin not found: " + fl[0]);
                ci.add(new VisualElement("Probe").setPos(at).setAttribute(Keys.LABEL, fl[1])
                        .setShapeFactory(cl.getShapeFactory()));
            }

        Model m = cl.createModel();
        if (!prog.equals("-"))      // "-" : ROMs load their own files (autoReload)
            new ProgramMemoryLoader(new File(prog), true).preInit(m);

        // prompts and banners are counted as the characters arrive, so a long output is not rescanned every half
        String banner = System.getProperty("banner", "");
        StringBuilder out = new StringBuilder();
        int[] seen = {0, 0};    // prompts ("> "), banners
        for (Terminal t : m.findNode(Terminal.class))
            t.setTerminalInterface(new TerminalInterface() {
                public void addChar(char c) {
                    out.append(c); System.out.print(c); System.out.flush();
                    if (endsWith(out, "> ")) seen[0]++;
                    if (!banner.isEmpty() && endsWith(out, banner)) seen[1]++;
                }
                public String getText() { return out.toString(); }
            });
        StringBuilder kb = new StringBuilder();
        List<Keyboard> kbds = m.findNode(Keyboard.class);
        for (Keyboard k : m.findNode(Keyboard.class))
            k.setKeyboard(new Keyboard.KeyboardInterface() {
                public int getChar() { return kb.length() > 0 ? kb.charAt(0) : 0; }
                public void deleteFirstChar() { if (kb.length() > 0) kb.deleteCharAt(0); }
            });

        String zseed = System.getProperty("zseed");
        if (zseed != null) System.err.println("zseed: " + seedHighZ(m, zseed) + " values");
        m.init();
        for (String kv : System.getProperty("set", "E=1,run=1").split(",")) {
            String[] p = kv.split("=");
            m.getInput(p[0]).setValue(Long.parseLong(p[1]));
        }
        m.doStep();

        PrintWriter csv = null;
        List<Signal> sigs = m.getSignals();
        if (System.getProperty("cols") != null) {   // cols=a,b,c : dump only these signals
            Set<String> want = new HashSet<>(Arrays.asList(System.getProperty("cols").split(",")));
            List<Signal> f = new ArrayList<>();
            for (Signal s : sigs) if (want.contains(s.getName())) f.add(s);
            sigs = f;
        }
        String csvName = System.getProperty("csv");
        long csvFrom = Long.parseLong(System.getProperty("csvFrom", "0"));
        int csvKey = Integer.parseInt(System.getProperty("csvKey", "0"));
        if (csvName != null) {
            csv = new PrintWriter(new BufferedWriter(new FileWriter(csvName)));
            StringBuilder h = new StringBuilder("half");
            for (Signal s : sigs) h.append(',').append(s.getName());
            csv.println(h);
        }
        boolean dumping = csvName != null && csvKey == 0;
        PrintWriter delta = null;
        String[] last = new String[sigs.size()];
        if (System.getProperty("delta") != null) {
            delta = new PrintWriter(new BufferedWriter(new FileWriter(System.getProperty("delta"))));
            StringBuilder hd = new StringBuilder();
            for (Signal s : sigs) hd.append(s.getName()).append(' ');
            delta.println(hd.toString().trim());
        }
        PrintWriter hash = System.getProperty("hash") == null ? null
                : new PrintWriter(new BufferedWriter(new FileWriter(System.getProperty("hash"))));

        boolean stopRestart = !System.getProperty("stopRestart", "1").equals("0");
        // stall : stop when the watched signals (PC and the commit flags) keep their value this many halves
        // (halt, deadlock); idle : stop after this many halves without terminal output (e.g. jmp $ at the end)
        long stall = Long.parseLong(System.getProperty("stall", "2000"));
        long idle = Long.parseLong(System.getProperty("idle", "0"));
        List<ObservableValue> watch = new ArrayList<>();
        Set<String> watchNames = new LinkedHashSet<>(Arrays.asList(System.getProperty("stallCols", "PC,com1,com2").split(",")));
        for (Signal s : m.getSignals()) if (watchNames.contains(s.getName())) watch.add(s.getValue());
        if (stall > 0 && watch.isEmpty()) System.err.println("stall: none of " + watchNames + " found, stall check off");
        ObservableValue pcSig = null;
        for (Signal s : m.getSignals()) if (s.getName().equals("PC")) pcSig = s.getValue();
        long watchHash = 0, lastChange = 0, lastOut = 0;
        int outLen = 0;
        String stop = "";
        ObservableValue c = m.getClocks().get(0).getClockOutput();
        long t0 = System.currentTimeMillis();
        int ki = 0, prompts = 0;
        long restartAt = -1, h;
        TreeMap<Long, String> at = new TreeMap<>();
        if (System.getProperty("at") != null)
            for (String e : System.getProperty("at").split(";")) {
                String[] p = e.split(":", 2);
                at.put(Long.parseLong(p[0]), p[1]);
            }
        for (h = 0; h < maxHalf; h++) {
            if (at.containsKey(h))
                for (String kv : at.get(h).split(",")) {
                    String[] p = kv.split("=");
                    m.getInput(p[0]).setValue(Long.parseLong(p[1]));
                }
            c.setBool(!c.getBool());
            m.doStep();
            if (dumping && h >= csvFrom) {
                StringBuilder r = new StringBuilder().append(h);
                for (Signal s : sigs) r.append(',').append(fmt(s.getValue()));
                csv.println(r);
            }
            if (delta != null)
                for (int i = 0; i < last.length; i++) {
                    String v = fmt(sigs.get(i).getValue());
                    if (!v.equals(last[i])) { delta.println(h + " " + i + " " + v); last[i] = v; }
                }
            if (hash != null) {   // FNV-1a over value and high-Z of every signal
                long x = 0xcbf29ce484222325L;
                for (Signal s : sigs) {
                    ObservableValue v = s.getValue();
                    x = (x ^ v.getValueHighZIsZero()) * 0x100000001b3L;   // does not draw random bits
                    x = (x ^ v.getHighZ()) * 0x100000001b3L;
                }
                hash.println(h + " " + Long.toHexString(x));
            }
            if (seen[0] > prompts && endsWith(out, "> ")) {
                prompts = seen[0];
                if (ki < items.length) {
                    kb.append(items[ki++]).append(lines ? "\n" : "");
                    for (Keyboard k : kbds) k.hasChanged();   // like KeyboardDialog: re-evaluate av
                    m.doStep();
                    if (csvName != null && ki == csvKey) dumping = true;
                } else { stop = " keys used up"; break; }
            }
            if (restartAt < 0 && seen[1] > 1) {
                restartAt = h;
                System.out.print("\n[RESTART at half " + h + "]\n");
            }
            if (stopRestart && restartAt >= 0 && h > restartAt + 200) { stop = " restart"; break; }
            if (out.length() != outLen) { outLen = out.length(); lastOut = h; }
            if (idle > 0 && h - lastOut >= idle) { stop = " idle " + idle; break; }
            if (stall > 0 && !watch.isEmpty()) {
                long x = 0xcbf29ce484222325L;
                for (ObservableValue v : watch)
                    x = ((x ^ v.getValueHighZIsZero()) * 0x100000001b3L ^ v.getHighZ()) * 0x100000001b3L;
                if (x != watchHash) { watchHash = x; lastChange = h; }
                else if (h - lastChange >= stall) { stop = " stall " + stall; break; }
            }
        }
        if (csv != null) csv.close();
        if (hash != null) hash.close();
        if (delta != null) delta.close();
        System.out.println("\n[half=" + h + " keysFed=" + ki + " ms=" + (System.currentTimeMillis() - t0)
                + (restartAt >= 0 ? " RESTART@" + restartAt : "")
                + (pcSig != null ? " PC=" + fmt(pcSig) : "") + (stop.isEmpty() ? "" : " stop:" + stop) + "]");
    }

    static boolean endsWith(StringBuilder s, String suffix) {
        int n = s.length() - suffix.length();
        return n >= 0 && s.indexOf(suffix, n) == n;
    }

    static String fmt(ObservableValue v) {
        if (v.isHighZ()) return "Z";
        if (v.getHighZ() != 0)      // partly floating: value bits + z mask (getValue would draw random bits)
            return Long.toHexString(v.getValueHighZIsZero()) + "z" + Long.toHexString(v.getHighZ());
        return Long.toHexString(v.getValue());
    }

    // Replace ObservableValue.random of every value reachable from the model nodes.
    static int seedHighZ(Model m, String mode) throws Exception {
        java.lang.reflect.Field rf = ObservableValue.class.getDeclaredField("random");
        rf.setAccessible(true);
        long base = mode.equals("zero") ? 0 : Long.parseLong(mode);
        Set<Object> seen = Collections.newSetFromMap(new IdentityHashMap<>());
        int[] n = {0};
        for (Object node : m.getNodes()) walk(node, 0, seen, rf, mode.equals("zero"), base, n);
        return n[0];
    }

    static void walk(Object o, int depth, Set<Object> seen, java.lang.reflect.Field rf, boolean zero, long base, int[] n) throws Exception {
        if (o == null || depth > 4 || !seen.add(o)) return;
        if (o instanceof ObservableValue) {
            rf.set(o, zero ? new Random() { protected int next(int bits) { return 0; } } : new Random(base * 1000003 + n[0]));
            n[0]++;
            return;
        }
        if (o instanceof Object[]) { for (Object e : (Object[]) o) walk(e, depth + 1, seen, rf, zero, base, n); return; }
        if (o instanceof Iterable) { for (Object e : (Iterable<?>) o) walk(e, depth + 1, seen, rf, zero, base, n); return; }
        if (!o.getClass().getName().startsWith("de.neemann.digital.core")) return;
        for (Class<?> c = o.getClass(); c != null && c != Object.class; c = c.getSuperclass())
            for (java.lang.reflect.Field f : c.getDeclaredFields()) {
                if (java.lang.reflect.Modifier.isStatic(f.getModifiers()) || f.getType().isPrimitive()) continue;
                f.setAccessible(true);
                walk(f.get(o), depth + 1, seen, rf, zero, base, n);
            }
    }
}
