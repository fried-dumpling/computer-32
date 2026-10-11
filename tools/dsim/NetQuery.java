// Netlist query for a Digital circuit (one level, no flattening).
//
//   java -cp "Digital.jar;build" NetQuery <circuit.dig> net <NetName>...    pins on the nets with those tunnel names
//   java -cp "Digital.jar;build" NetQuery <circuit.dig> at <x,y>...         element(s) at a position and every pin's net
//   java -cp "Digital.jar;build" NetQuery <circuit.dig> sub <file.dig> net|at ...   same, inside a subcircuit
//   java -cp "Digital.jar;build" NetQuery <circuit.dig> label <Label>...     elements with that label and their pin nets
//
// a net is printed as its tunnel names, or "@x,y" (first point) when it has none.
import de.neemann.digital.cli.CircuitLoader;
import de.neemann.digital.core.element.Keys;
import de.neemann.digital.draw.elements.*;
import de.neemann.digital.draw.library.ElementTypeDescriptionCustom;
import de.neemann.digital.draw.model.Net;
import de.neemann.digital.draw.model.NetList;

import java.io.File;
import java.util.*;

public class NetQuery {
    static NetList nl;
    static Map<Net, List<String>> pinsOf = new HashMap<>();
    static Map<String, List<Net>> byName = new HashMap<>();

    public static void main(String[] a) throws Exception {
        CircuitLoader cl = new CircuitLoader(new File(a[0]));
        Circuit c = cl.getCircuit();
        int i = 1;
        if (a[i].equals("sub")) {
            c = ((ElementTypeDescriptionCustom) cl.getLibrary().getElementType(a[i + 1])).getCircuit();
            i += 2;
        }
        nl = new NetList(c);
        for (Net n : nl) for (String l : n.getLabels()) byName.computeIfAbsent(l, k -> new ArrayList<>()).add(n);
        for (VisualElement ve : c.getElements()) {
            if (ve.getElementName().equals("Tunnel")) continue;
            for (Pin p : ve.getPins()) {
                Net n = nl.getNetOfPos(p.getPos());
                if (n != null) pinsOf.computeIfAbsent(n, k -> new ArrayList<>()).add(desc(ve) + "." + p.getName() + "(" + p.getDirection() + ")");
            }
        }
        String mode = a[i++];
        for (; i < a.length; i++) {
            if (mode.equals("net")) {
                List<Net> ns = byName.get(a[i]);
                if (ns == null) { System.out.println(a[i] + ": no such net"); continue; }
                for (Net n : ns) System.out.println(name(n) + " :\n    " + String.join("\n    ", pinsOf.getOrDefault(n, List.of())));
            } else if (mode.equals("at") || mode.equals("label")) {
                for (VisualElement ve : c.getElements()) {
                    boolean hit = mode.equals("at") ? (ve.getPos().x + "," + ve.getPos().y).equals(a[i])
                            : a[i].equals(ve.getElementAttributes().getLabel());
                    if (!hit) continue;
                    System.out.println(desc(ve));
                    for (Pin p : ve.getPins()) {
                        Net n = nl.getNetOfPos(p.getPos());
                        System.out.println("    " + p.getName() + "(" + p.getDirection() + ") -> " + (n == null ? "open" : name(n)
                                + "   [" + String.join(", ", pinsOf.getOrDefault(n, List.of())) + "]"));
                    }
                }
            }
        }
    }

    static String desc(VisualElement ve) {
        String l = ve.getElementAttributes().getLabel();
        String extra = "";
        if (ve.getElementName().equals("Const")) extra = "=" + ve.getElementAttributes().get(Keys.VALUE);
        if (ve.getElementName().equals("Splitter"))
            extra = "[" + ve.getElementAttributes().get(Keys.INPUT_SPLIT) + ">" + ve.getElementAttributes().get(Keys.OUTPUT_SPLIT) + "]";
        return ve.getElementName() + (l.isEmpty() ? "" : "'" + l + "'") + extra + "@" + ve.getPos().x + "," + ve.getPos().y;
    }

    static String name(Net n) {
        if (!n.getLabels().isEmpty()) return String.join("|", n.getLabels());
        for (Pin p : n.getPins()) return "@" + p.getPos().x + "," + p.getPos().y;
        if (n.getWires() != null) for (Wire w : n.getWires()) return "@" + w.p1.x + "," + w.p1.y;
        return "@?";
    }
}
