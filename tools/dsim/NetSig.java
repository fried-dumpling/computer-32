// Print every non-tunnel pin with the net it is on, one line per pin:
//   <element>@x,y.<pin>\t<net id>\t<tunnel names of the net, a|b>
// (net id = index of the merged net). Used to compare two circuits structurally.
//   java -cp "Digital.jar;build" NetSig <circuit.dig>
import de.neemann.digital.cli.CircuitLoader;
import de.neemann.digital.draw.elements.*;
import de.neemann.digital.draw.model.Net;
import de.neemann.digital.draw.model.NetList;

import java.io.File;
import java.util.*;

public class NetSig {
    public static void main(String[] a) throws Exception {
        CircuitLoader cl = new CircuitLoader(new File(a[0]));
        Circuit c = cl.getCircuit();
        NetList nl = new NetList(c);
        Map<Net, Integer> id = new IdentityHashMap<>();
        for (VisualElement ve : c.getElements()) {
            if (ve.getElementName().equals("Tunnel")) continue;
            for (Pin p : ve.getPins()) {
                Net n = nl.getNetOfPos(p.getPos());
                String nid = n == null ? "open" : "n" + id.computeIfAbsent(n, k -> id.size());
                String labels = n == null ? "" : String.join("|", new TreeSet<>(n.getLabels()));
                System.out.println(ve.getElementName() + "@" + ve.getPos().x + "," + ve.getPos().y + "." + p.getName() + "\t" + nid + "\t" + labels);
            }
        }
    }
}
