/**
  * This is a little helper class which holds additional information about
  * a dataset or global attribute.
  *
  * @author Mark Koennecke, October 2000
  *
  * @see NeXusFileInterface.
  *
  * copyright: see acompanying COPYRIGHT file.
  */
package neutron.nexus;

public class AttributeEntry {
    /**
      * length is the length of the attribute.
      * type is the number type of the attribute.
      */
  public int length, type;
}