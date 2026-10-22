// In-memory circuit patches for experiments (the .dig files are not touched).
// Used by Sim with -Dpatch=name[,name...].
//
//   scsfd : speculative stack pointer update on flush.
//           original : scs_i = clr ? ROB-SC.D3 (FD) : (SCS or ISC) +- SCN
//           patched  : scs_i = (clr ? FD : (SCS or ISC)) +- SCN
//           so a stack op dispatched from the RS in the same half cycle as a flush
//           adds its +-4 on top of the rollback value instead of being dropped.
import de.neemann.digital.cli.CircuitLoader;
import de.neemann.digital.core.element.Keys;
import de.neemann.digital.draw.elements.Circuit;
import de.neemann.digital.draw.elements.Pin;
import de.neemann.digital.draw.elements.VisualElement;
import de.neemann.digital.draw.graphics.Vector;

import java.util.HashMap;
import java.util.Map;

public class Patch {
    static CircuitLoader cl;

    public static void apply(CircuitLoader loader, String names) throws Exception {
        cl = loader;
        for (String n : names.split(",")) {
            if (n.equals("scsfd")) scsfd(cl.getCircuit());
            else if (n.equals("rsgate")) rsgate();
            else if (n.equals("wfprio")) {
                Circuit w = ((de.neemann.digital.draw.library.ElementTypeDescriptionCustom)
                        cl.getLibrary().getElementType("inst-wfile.dig")).getCircuit();
                wfprio(w, "FL", 2420, -360);
                wfprio(w, "SC", 2340, -620);
                // RS : FL_inst / SC_inst carry the new id only on a real insertion (OFL/OSC & E & !clr)
                Circuit rs = ((de.neemann.digital.draw.library.ElementTypeDescriptionCustom)
                        cl.getLibrary().getElementType("res-station-debug.dig")).getCircuit();
                tunnel(rs, "P_ins_ok", pin(find(rs, "And", -3340, 1540), "out"));   // E & !clr
                gateSel(rs, find(rs, "Multiplexer", -3500, 1860), "OFL");
                gateSel(rs, find(rs, "Multiplexer", -3500, 1800), "OSC");
            }
            else throw new IllegalArgumentException("unknown patch " + n);
            System.out.println("[patch " + n + " applied]");
        }
    }

    static void scsfd(Circuit c) throws Exception {
        VisualElement selMux = find(c, "Multiplexer", 2460, -180);   // SCS / ISC -> adder.a
        VisualElement outMux = find(c, "Multiplexer", 2680, -160);   // clr ? FD : adder.s -> scs_i
        Vector adderA = pin(selMux, "out"), adderS = pin(outMux, "in_0");
        // remove both muxes
        c.delete(selMux);
        c.delete(outMux);
        // adder.s drives scs_i directly
        tunnel(c, "scs_i", adderS);
        // new muxes in free space : m1 = ISCCM ? ISC : SCS ; m2 = clr ? FD : m1 -> adder.a
        VisualElement m1 = mux(c, new Vector(20000, 21000));
        VisualElement m2 = mux(c, new Vector(20000, 21400));
        tunnel(c, "CT_ISCCM", pin(m1, "sel"));
        tunnel(c, "SCS", pin(m1, "in_0"));
        tunnel(c, "ISC", pin(m1, "in_1"));
        tunnel(c, "P_scs_a0", pin(m1, "out"));
        tunnel(c, "clr", pin(m2, "sel"));
        tunnel(c, "P_scs_a0", pin(m2, "in_0"));
        tunnel(c, "CT_SC-_FD", pin(m2, "in_1"));
        tunnel(c, "P_scs_a", pin(m2, "out"));
        tunnel(c, "P_scs_a", adderA);
    }

    // inst-wfile FL / SC tag register: the data mux (sel = X_M) picks "commit clear"
    // (none or rcom_id) even when a new producer is inserted in the same cycle.
    // patched sel = X_M & !(X_sel & !X_i[5])   (X_i[5] = none bit, 0 when inserting)
    static int uniq = 0;
    static void wfprio(Circuit c, String x, int mx, int my) throws Exception {
        VisualElement m = find(c, "Multiplexer", mx, my);
        Vector sel = pin(m, "sel");
        // unplug sel : move the mux 1 grid right/left is not possible, so rebuild it
        Vector in0 = pin(m, "in_0"), in1 = pin(m, "in_1"), out = pin(m, "out");
        Object bits = m.getElementAttributes().get(Keys.BITS);
        c.delete(m);
        String p = "P_" + x + "_" + (uniq++) + "_";
        VisualElement nm = mux(c, new Vector(30000, 30000 + uniq * 400));
        nm.setAttribute(Keys.BITS, (Integer) bits);
        tunnel(c, p + "in0", pin(nm, "in_0")); tunnel(c, p + "in0", in0);
        tunnel(c, p + "in1", pin(nm, "in_1")); tunnel(c, p + "in1", in1);
        tunnel(c, p + "out", pin(nm, "out")); tunnel(c, p + "out", out);
        // none bit of X_i
        VisualElement sp = add(c, new VisualElement("Splitter").setPos(new Vector(32000, 30000 + uniq * 400))
                .setAttribute(Keys.INPUT_SPLIT, "6").setAttribute(Keys.OUTPUT_SPLIT, "5,1"));
        tunnel(c, x + "_i", pin(sp, "0-5"));
        tunnel(c, p + "none", pin(sp, "5"));
        VisualElement n1 = add(c, new VisualElement("Not").setPos(new Vector(33000, 30000 + uniq * 400)));
        tunnel(c, p + "none", pin(n1, "in")); tunnel(c, p + "valid", pin(n1, "out"));
        VisualElement a1 = add(c, new VisualElement("And").setPos(new Vector(34000, 30000 + uniq * 400)));
        tunnel(c, x + "_sel", pin(a1, "In_1")); tunnel(c, p + "valid", pin(a1, "In_2")); tunnel(c, p + "ins", pin(a1, "out"));
        VisualElement n2 = add(c, new VisualElement("Not").setPos(new Vector(35000, 30000 + uniq * 400)));
        tunnel(c, p + "ins", pin(n2, "in")); tunnel(c, p + "nins", pin(n2, "out"));
        VisualElement a2 = add(c, new VisualElement("And").setPos(new Vector(36000, 30000 + uniq * 400)));
        tunnel(c, x + "_M", pin(a2, "In_1")); tunnel(c, p + "nins", pin(a2, "In_2")); tunnel(c, p + "sel", pin(a2, "out"));
        tunnel(c, p + "sel", pin(nm, "sel"));
    }

    // only the RS half of wfprio (FL_inst / SC_inst sel = OFL/OSC & E & !clr)
    static void rsgate() throws Exception {
        Circuit rs = ((de.neemann.digital.draw.library.ElementTypeDescriptionCustom)
                cl.getLibrary().getElementType("res-station-debug.dig")).getCircuit();
        tunnel(rs, "P_ins_ok", pin(find(rs, "And", -3340, 1540), "out"));
        gateSel(rs, find(rs, "Multiplexer", -3500, 1860), "OFL");
        gateSel(rs, find(rs, "Multiplexer", -3500, 1800), "OSC");
    }

    // rebuild a 2-input mux with sel = <selNet> & P_ins_ok
    static void gateSel(Circuit c, VisualElement m, String selNet) throws Exception {
        Vector in0 = pin(m, "in_0"), in1 = pin(m, "in_1"), out = pin(m, "out");
        Object bits = m.getElementAttributes().get(Keys.BITS);
        c.delete(m);
        String p = "P_g" + (uniq++) + "_";
        VisualElement nm = mux(c, new Vector(-30000, 30000 + uniq * 400));
        nm.setAttribute(Keys.BITS, (Integer) bits);
        tunnel(c, p + "in0", pin(nm, "in_0")); tunnel(c, p + "in0", in0);
        tunnel(c, p + "in1", pin(nm, "in_1")); tunnel(c, p + "in1", in1);
        tunnel(c, p + "out", pin(nm, "out")); tunnel(c, p + "out", out);
        VisualElement a = add(c, new VisualElement("And").setPos(new Vector(-32000, 30000 + uniq * 400)));
        tunnel(c, selNet, pin(a, "In_1")); tunnel(c, "P_ins_ok", pin(a, "In_2")); tunnel(c, p + "sel", pin(a, "out"));
        tunnel(c, p + "sel", pin(nm, "sel"));
    }

    static VisualElement add(Circuit c, VisualElement ve) {
        ve.setShapeFactory(cl.getShapeFactory());
        c.add(ve);
        return ve;
    }

    static VisualElement find(Circuit c, String name, int x, int y) {
        for (VisualElement ve : c.getElements())
            if (ve.getElementName().equals(name) && ve.getPos().x == x && ve.getPos().y == y) return ve;
        throw new IllegalStateException(name + "@" + x + "," + y + " not found");
    }

    static Vector pin(VisualElement ve, String name) {
        for (Pin p : ve.getPins()) if (p.getName().equals(name)) return p.getPos();
        throw new IllegalStateException("pin " + name);
    }

    static VisualElement mux(Circuit c, Vector pos) {
        VisualElement ve = new VisualElement("Multiplexer").setPos(pos)
                .setAttribute(Keys.BITS, 32).setShapeFactory(cl.getShapeFactory());
        c.add(ve);
        return ve;
    }

    static void tunnel(Circuit c, String net, Vector pos) {
        c.add(new VisualElement("Tunnel").setPos(pos).setAttribute(Keys.NETNAME, net)
                .setShapeFactory(cl.getShapeFactory()));
    }
}
