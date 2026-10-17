// Headless runner for the Digital CPU circuit (uses Digital.jar as a library).
//
//   javac -cp Digital.jar -d build Sim.java
//   java -cp "Digital.jar;build" Sim <circuit.dig> <program.bin> <keys> [maxHalfCycles]
//
// keys     : one key is fed each time the program prints "> " (\n allowed)
// options (-Dname=value, before Sim) :
//   dip=x,y;x,y    DipSwitches turned on before the model is built
//                  (default -580,-1640 = clock mux select -> Clock)
//   set=E=1,run=1  top-level inputs
//   csv=file       dump every model signal (probes, In/Out) each half cycle
//   csvFrom=N      start the dump at half cycle N (default 0)
//   csvKey=N       or start the dump when the N-th key (1-based) is fed
//   banner=text    text printed once at reset; seeing it again = restart (default "m8 ")
//   stopRestart=1  stop shortly after a restart is detected (default 1)
//   patch=name     in-memory circuit patch from Patch.java (e.g. scsfd)
//   probe=f.dig@x,y=name;...  extra Probe on an unnamed net point (f = top or a subcircuit file)
//   cols=a,b,...   with csv : dump only these signals
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
        String[] items = lines ? keys.split("\\|") : keys.split("");
        long maxHalf = a.length > 3 ? Long.parseLong(a[3]) : 2_000_000;

        CircuitLoader cl = new CircuitLoader(new File(dig));
        Set<String> dips = new HashSet<>(Arrays.asList(System.getProperty("dip", "-580,-1640").split(";")));
        for (VisualElement ve : cl.getCircuit().getElements())
            if (ve.getElementName().equals("DipSwitch") && dips.contains(ve.getPos().x + "," + ve.getPos().y))
                ve.getElementAttributes().set(Keys.DIP_DEFAULT, true);

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

        StringBuilder out = new StringBuilder();
        for (Terminal t : m.findNode(Terminal.class))
            t.setTerminalInterface(new TerminalInterface() {
                public void addChar(char c) { out.append(c); System.out.print(c); System.out.flush(); }
                public String getText() { return out.toString(); }
            });
        StringBuilder kb = new StringBuilder();
        List<Keyboard> kbds = m.findNode(Keyboard.class);
        for (Keyboard k : m.findNode(Keyboard.class))
            k.setKeyboard(new Keyboard.KeyboardInterface() {
                public int getChar() { return kb.length() > 0 ? kb.charAt(0) : 0; }
                public void deleteFirstChar() { if (kb.length() > 0) kb.deleteCharAt(0); }
            });

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

        String banner = System.getProperty("banner", "m8 ");
        boolean stopRestart = !System.getProperty("stopRestart", "1").equals("0");
        ObservableValue c = m.getClocks().get(0).getClockOutput();
        long t0 = System.currentTimeMillis();
        int ki = 0, prompts = 0;
        long restartAt = -1, h;
        for (h = 0; h < maxHalf; h++) {
            c.setBool(!c.getBool());
            m.doStep();
            if (dumping && h >= csvFrom) {
                StringBuilder r = new StringBuilder().append(h);
                for (Signal s : sigs) r.append(',').append(fmt(s.getValue()));
                csv.println(r);
            }
            String o = out.toString();
            if (o.endsWith("> ") && countOf(o, "> ") > prompts) {
                prompts = countOf(o, "> ");
                if (ki < items.length) {
                    kb.append(items[ki++]).append(lines ? "\n" : "");
                    for (Keyboard k : kbds) k.hasChanged();   // like KeyboardDialog: re-evaluate av
                    m.doStep();
                    if (csvName != null && ki == csvKey) dumping = true;
                } else break;
            }
            if (restartAt < 0 && countOf(o, banner) > 1) {
                restartAt = h;
                System.out.print("\n[RESTART at half " + h + "]\n");
            }
            if (stopRestart && restartAt >= 0 && h > restartAt + 200) break;
        }
        if (csv != null) csv.close();
        System.out.println("\n[half=" + h + " keysFed=" + ki + " ms=" + (System.currentTimeMillis() - t0)
                + (restartAt >= 0 ? " RESTART@" + restartAt : "") + "]");
    }

    static String fmt(ObservableValue v) {
        if (v.isHighZ()) return "Z";
        return Long.toHexString(v.getValue());
    }

    static int countOf(String s, String sub) {
        int n = 0, i = 0;
        while ((i = s.indexOf(sub, i)) >= 0) { n++; i += sub.length(); }
        return n;
    }
}
