package fr.gael.openjpeg.imageio;

import java.io.ByteArrayOutputStream;
import java.io.IOException;

import javax.imageio.stream.ImageInputStream;

import org.apache.log4j.Logger;

public class Utils
{
   private static final Logger LOGGER = Logger.getLogger (Utils.class);
   private static final int BUFFER_SIZE = 64 * 1024;

   /**
    * Converts a ImageInputStream to a byte array.
    *
    * @return a byte array containing the content of the stream.
    * @throws IOException if a error occurred during reading of stream.
    */
   public static byte[] imageInputStreamToByteArray (ImageInputStream iis)
         throws IOException
   {
      int read;
      byte[] buffer = new byte[BUFFER_SIZE];
      ByteArrayOutputStream baos = new ByteArrayOutputStream ();

      while ((read = iis.read (buffer, 0, BUFFER_SIZE)) != -1)
      {
         baos.write (buffer, 0, read);
      }

      iis.close ();

      return baos.toByteArray ();
   }

   private Utils ()
   {
   }
}
