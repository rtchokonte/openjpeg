package fr.gael.openjpeg;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.net.URISyntaxException;
import java.net.URL;
import java.nio.file.Paths;
import java.util.Arrays;

import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

public class TestOpenJpegDecoder
{
   private static final int TEMP_BUFFER_SIZE = 1024 * 1024;

   private String sourceFile = "test.jp2";
   private String workingFile;

   @BeforeClass
   public void init () throws
         IOException, ReflectiveOperationException, URISyntaxException
   {
      initWorkingFile ();
      addTestDirectoryToLibraryPath ();
   }

   private void initWorkingFile () throws IOException
   {
      InputStream is = ClassLoader.getSystemResourceAsStream (sourceFile);
      File w_file = File.createTempFile ("jp2k", ".jp2");
      w_file.deleteOnExit ();
      FileOutputStream fos = new FileOutputStream (w_file);
      try
      {
         byte[] tempBuffer = new byte[TEMP_BUFFER_SIZE];
         int bytesRead;
         while ((bytesRead = is.read (tempBuffer, 0, TEMP_BUFFER_SIZE)) != -1)
         {
            fos.write (tempBuffer, 0, bytesRead);
         }
      }
      finally
      {
         if (is != null)
         {
            is.close ();
         }
         if (fos != null)
         {
            fos.close ();
         }
      }
      workingFile = w_file.getPath ();
   }

   private void addTestDirectoryToLibraryPath ()
         throws URISyntaxException, ReflectiveOperationException
   {
      // retrieve test path location
      URL url = ClassLoader.getSystemResource (
            "libopenjp2." + getSystemLibExtension ());
      String testPath = Paths.get (url.toURI ()).getParent ().toString ();
      // add to ClassLoader
      Field usrPathField = ClassLoader.class.getDeclaredField ("usr_paths");
      usrPathField.setAccessible (true);

      String[] currentUsrPath = (String[]) usrPathField.get (null);
      String[] myUsrPath =
            Arrays.copyOf (currentUsrPath, (currentUsrPath.length + 1));
      myUsrPath[(myUsrPath.length - 1)] = testPath;
      usrPathField.set (null, myUsrPath);

      usrPathField.setAccessible (false);
   }

   private String getSystemLibExtension ()
   {
      String osName = System.getProperty ("os.name");

      if (osName.contains ("Linux"))
      {
         return "so";
      }
      if (osName.contains ("Windows"))
      {
         return "dll";
      }
      if (osName.contains ("Mac"))
      {
         return "dylib";
      }

      throw new UnsupportedOperationException (
            "Incompatible system: " + osName);
   }

   @Test
   public void internalGetHeader ()
   {
      OpenJpegDecoder decoder = new OpenJpegDecoder ();

      Assert.assertEquals (decoder.getWidth (), -1);
      Assert.assertEquals (decoder.getHeight (), -1);
      decoder.decodeHeader (workingFile);
      Assert.assertEquals (decoder.getWidth (), 1830);
      Assert.assertEquals (decoder.getHeight (), 1830);
   }
}