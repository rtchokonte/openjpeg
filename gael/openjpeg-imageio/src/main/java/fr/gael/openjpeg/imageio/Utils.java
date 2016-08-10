package fr.gael.openjpeg.imageio;

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
      if (iis.length () > Integer.MAX_VALUE)
      {
         throw new IOException ("Stream too large");
      }

      byte[] result = new byte[(int) iis.length ()];
      byte[] buffer = new byte[BUFFER_SIZE];
      int bytesRead;
      int offset = 0;

      while ((bytesRead = iis.read (buffer, 0, BUFFER_SIZE)) != -1)
      {
         System.arraycopy (buffer, 0, result, offset, bytesRead);
         offset = offset + bytesRead;
      }

      return result;
   }

   private Utils ()
   {
   }
}
