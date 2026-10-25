// Print the pin positions (relative to the element origin) of a subcircuit's shape.
//   java -cp "Digital.jar;build" Pins <any circuit in the dir> <sub.dig>...
import de.neemann.digital.cli.CircuitLoader;
import de.neemann.digital.draw.elements.*;

import java.io.File;

public class Pins {
    public static void main(String[] a) throws Exception {
        CircuitLoader cl = new CircuitLoader(new File(a[0]));
        for (int i = 1; i < a.length; i++) {
            VisualElement ve = new VisualElement(a[i]).setShapeFactory(cl.getLibrary().getShapeFactory());
            for (Pin p : ve.getPins()) System.out.println(a[i] + " " + p.getName() + " " + p.getPos().x + " " + p.getPos().y);
        }
    }
}
