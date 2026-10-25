// Find elements/wires to delete when stripping debug parts from a Digital circuit.
//
//   java -cp "Digital.jar;build" Strip <circuit.dig> <root>...
//     root: type:<ElementName>          every element of that type
//           label:<ElementName>:<Label> element with that label
//           left:<x>                    every element with pos.x < x
//           right:<x>                   every element with pos.x >= x
//   prints "E <name> <x> <y>" for each element and "W <x1> <y1> <x2> <y2>" for each wire to delete,
//   then "KEEP-DRIVEN <net>" for nets that lost all drivers but still have consumers (boundary signals).
//
// An element is dead when it is a root or when none of its output pins drives a live consumer
// (tunnels are transparent: nets are merged by name). In/Out/Clock are never removed unless a root.
// -Dnocascade=true removes only the roots (plus the tunnels and wires they leave unused).
import de.neemann.digital.cli.CircuitLoader;
import de.neemann.digital.core.element.PinDescription;
import de.neemann.digital.draw.elements.*;
import de.neemann.digital.draw.graphics.Vector;
import de.neemann.digital.draw.model.Net;
import de.neemann.digital.draw.model.NetList;

import java.io.File;
import java.util.*;

public class Strip {
    static final Set<String> KEEP = Set.of("In", "Out", "Clock", "Text", "Rectangle");

    public static void main(String[] a) throws Exception {
        CircuitLoader cl = new CircuitLoader(new File(a[0]));
        Circuit c = cl.getCircuit();
        NetList nl = new NetList(c);
        List<VisualElement> els = c.getElements();
        Set<VisualElement> dead = new HashSet<>();
        for (int i = 1; i < a.length; i++) {
            String[] r = a[i].split(":", 3);
            for (VisualElement ve : els) {
                boolean hit = switch (r[0]) {
                    case "type" -> ve.getElementName().equals(r[1]);
                    case "label" -> ve.getElementName().equals(r[1]) && r[2].equals(ve.getElementAttributes().getLabel());
                    case "left" -> ve.getPos().x < Integer.parseInt(r[1]);
                    case "right" -> ve.getPos().x >= Integer.parseInt(r[1]);
                    default -> throw new RuntimeException(a[i]);
                };
                if (hit) dead.add(ve);
            }
        }
        // consumers per merged net
        boolean changed = !Boolean.getBoolean("nocascade");
        while (changed) {
            changed = false;
            Map<Net, Integer> consumers = new HashMap<>();
            for (VisualElement ve : els) {
                if (dead.contains(ve) || ve.getElementName().equals("Tunnel")) continue;
                for (Pin p : ve.getPins()) {
                    if (p.getDirection() == PinDescription.Direction.output) continue;
                    Net n = nl.getNetOfPos(p.getPos());
                    if (n != null) consumers.merge(n, 1, Integer::sum);
                }
            }
            for (VisualElement ve : els) {
                String name = ve.getElementName();
                if (dead.contains(ve) || name.equals("Tunnel") || KEEP.contains(name)) continue;
                boolean hasOut = false, used = false;
                for (Pin p : ve.getPins()) {
                    if (p.getDirection() == PinDescription.Direction.input) continue;
                    hasOut = true;
                    Net n = nl.getNetOfPos(p.getPos());
                    if (n == null) continue;
                    int own = 0;
                    for (Pin q : ve.getPins())
                        if (q.getDirection() != PinDescription.Direction.output && nl.getNetOfPos(q.getPos()) == n) own++;
                    if (consumers.getOrDefault(n, 0) - own > 0) used = true;
                }
                if (hasOut && !used) { dead.add(ve); changed = true; }
            }
        }
        // live anchor points: pins of live non-tunnel elements
        Set<Long> anchor = new HashSet<>();
        for (VisualElement ve : els)
            if (!dead.contains(ve) && !ve.getElementName().equals("Tunnel"))
                for (Pin p : ve.getPins()) anchor.add(key(p.getPos()));
        // local nets (wires + points), without tunnel merging
        Map<Long, Long> uf = new HashMap<>();
        List<Wire> wires = new ArrayList<>(c.getWires());
        for (Wire w : wires) union(uf, key(w.p1), key(w.p2));
        Set<Long> liveRoot = new HashSet<>();
        for (long k : anchor) liveRoot.add(find(uf, k));
        // a local net joining tunnels of different names aliases those nets: keep it
        Map<Long, Set<String>> tunNames = new HashMap<>();
        for (VisualElement ve : els)
            if (ve.getElementName().equals("Tunnel"))
                tunNames.computeIfAbsent(find(uf, key(ve.getPos())), k -> new HashSet<>()).add(ve.getElementAttributes().get(de.neemann.digital.core.element.Keys.NETNAME));
        for (Map.Entry<Long, Set<String>> e : tunNames.entrySet()) if (e.getValue().size() > 1) liveRoot.add(e.getKey());
        Set<Wire> deadW = new HashSet<>();
        for (Wire w : wires) if (!liveRoot.contains(find(uf, key(w.p1)))) deadW.add(w);
        for (VisualElement ve : els)
            if (ve.getElementName().equals("Tunnel") && !liveRoot.contains(find(uf, key(ve.getPos())))) dead.add(ve);
        // prune dangling stubs: wire end of degree 1 that is not a live pin or a kept tunnel
        Set<Long> tun = new HashSet<>();
        for (VisualElement ve : els)
            if (ve.getElementName().equals("Tunnel") && !dead.contains(ve)) tun.add(key(ve.getPos()));
        changed = true;
        while (changed) {
            changed = false;
            Map<Long, Integer> deg = new HashMap<>();
            for (Wire w : wires) if (!deadW.contains(w)) { deg.merge(key(w.p1), 1, Integer::sum); deg.merge(key(w.p2), 1, Integer::sum); }
            for (Wire w : wires) {
                if (deadW.contains(w)) continue;
                for (Vector v : new Vector[]{w.p1, w.p2}) {
                    long k = key(v);
                    if (deg.get(k) == 1 && !anchor.contains(k) && !tun.contains(k)) { deadW.add(w); changed = true; break; }
                }
            }
        }
        for (VisualElement ve : els)
            if (dead.contains(ve)) System.out.println("E " + ve.getElementName() + " " + ve.getPos().x + " " + ve.getPos().y);
        for (Wire w : deadW) System.out.println("W " + w.p1.x + " " + w.p1.y + " " + w.p2.x + " " + w.p2.y);
        // nets that keep live consumers but lost every driver
        for (Net n : nl) {
            boolean drv = false, cons = false, lostDrv = false;
            for (VisualElement ve : els) {
                if (ve.getElementName().equals("Tunnel")) continue;
                for (Pin p : ve.getPins()) {
                    if (nl.getNetOfPos(p.getPos()) != n) continue;
                    boolean isDrv = p.getDirection() != PinDescription.Direction.input;
                    boolean isCons = p.getDirection() != PinDescription.Direction.output;
                    if (dead.contains(ve)) { if (isDrv) lostDrv = true; }
                    else { if (isDrv) drv = true; if (isCons) cons = true; }
                }
            }
            if (lostDrv && cons && !drv) System.out.println("KEEP-DRIVEN " + (n.getLabels().isEmpty() ? "?" : String.join("|", n.getLabels())));
        }
    }

    static long key(Vector v) { return ((long) v.x << 32) ^ (v.y & 0xffffffffL); }

    static long find(Map<Long, Long> uf, long k) {
        Long p = uf.get(k);
        if (p == null || p == k) return k;
        long r = find(uf, p);
        uf.put(k, r);
        return r;
    }

    static void union(Map<Long, Long> uf, long a, long b) {
        long ra = find(uf, a), rb = find(uf, b);
        if (ra != rb) uf.put(ra, rb);
    }
}
