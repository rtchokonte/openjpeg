package fr.gael.openjpeg.imageio;

import java.awt.Point;
import java.awt.Transparency;
import java.awt.color.ColorSpace;
import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.awt.image.ComponentColorModel;
import java.awt.image.DataBuffer;
import java.awt.image.DataBufferByte;
import java.awt.image.DataBufferInt;
import java.awt.image.DataBufferUShort;
import java.awt.image.DirectColorModel;
import java.awt.image.Raster;
import java.awt.image.SampleModel;
import java.awt.image.SinglePixelPackedSampleModel;
import java.awt.image.WritableRaster;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import fr.gael.openjpeg.OpenJpegDecoder;
import javax.imageio.ImageReadParam;
import javax.imageio.ImageReader;
import javax.imageio.ImageTypeSpecifier;
import javax.imageio.metadata.IIOMetadata;
import javax.imageio.stream.ImageInputStream;
import org.apache.log4j.Logger;

public class OpenJpegImageReader extends ImageReader
{
   private static final Logger LOGGER =
         Logger.getLogger (OpenJpegImageReader.class);

   private File inputFile = null;
   private String filePath = null;
   private int numImages = 1;
   private OpenJpegDecoder decoder;
   private List<OpenJpegCodeStreamProperty> codeStreams;

   public OpenJpegImageReader (OpenJpegImageReaderSpi readerSpi)
   {
      super (readerSpi);
      this.decoder = new OpenJpegDecoder ();
      this.codeStreams = new ArrayList<> ();
   }

   @Override
   public int getNumImages (boolean allowSearch) throws IOException
   {
      return numImages;
   }

   @Override
   public int getWidth (int imageIndex) throws IOException
   {
      checkImageIndex (imageIndex);
      return codeStreams.get (imageIndex).getWidth ();
   }

   @Override
   public int getHeight (int imageIndex) throws IOException
   {
      checkImageIndex (imageIndex);
      return codeStreams.get (imageIndex).getHeight ();
   }

   @Override
   public Iterator<ImageTypeSpecifier> getImageTypes (int imageIndex)
         throws IOException
   {
      checkImageIndex (imageIndex);

      List<ImageTypeSpecifier> result = new ArrayList<> ();
      OpenJpegCodeStreamProperty codeStream = codeStreams.get (imageIndex);
      ImageTypeSpecifier typeSpecifier = new ImageTypeSpecifier (
            codeStream.getColorModel (), codeStream.getSampleModel ());

      result.add (typeSpecifier);
      return result.iterator ();
   }

   @Override
   public IIOMetadata getStreamMetadata () throws IOException
   {
      throw new UnsupportedOperationException ("Not yet implemented");
   }

   @Override
   public IIOMetadata getImageMetadata (int imageIndex) throws IOException
   {
      throw new UnsupportedOperationException ("Not yet implemented");
   }

   @Override
   public BufferedImage read (int imageIndex, ImageReadParam param)
         throws IOException
   {
      checkImageIndex (imageIndex);

      decoder.decode (filePath);
      decoder.setByteInputStream (null);

      int width = decoder.getWidth ();
      int height = decoder.getHeight ();
      BufferedImage bufferedImage = null;

      if (decoder.getImage24 () != null)
      {
         int[] buffer = decoder.getImage24 ();
         int[] bitMasks = new int[]{0xFF0000, 0xFF00, 0xFF};

         SinglePixelPackedSampleModel sm = new SinglePixelPackedSampleModel (
               DataBuffer.TYPE_INT, width, height, bitMasks);
         DataBufferInt db = new DataBufferInt (buffer, buffer.length);
         WritableRaster wr = Raster.createWritableRaster (sm, db, new Point ());
         ColorModel colorModel = new DirectColorModel (
               24, 0xFF0000, 0xFF00, 0xFF);
         bufferedImage = new BufferedImage (colorModel, wr, false, null);
      }
      else if (decoder.getImage16 () != null)
      {
         short[] buffer = decoder.getImage16 ();
         int[] bits = {16};

         ColorModel colorModel = new ComponentColorModel (
               ColorSpace.getInstance (ColorSpace.CS_GRAY), bits, false,
               false, Transparency.OPAQUE, DataBuffer.TYPE_USHORT);
         SampleModel sm = colorModel.createCompatibleSampleModel (width, height);
         DataBufferUShort db = new DataBufferUShort (buffer, width * height * 2);
         WritableRaster wr = Raster.createWritableRaster (sm, db, null);
         bufferedImage = new BufferedImage (colorModel, wr, false, null);
      }
      else if (decoder.getImage8 () != null)
      {
         byte[] buffer = decoder.getImage8 ();
         int[] bits = {8};

         ColorModel colorModel = new ComponentColorModel (
               ColorSpace.getInstance (ColorSpace.CS_GRAY), bits, false,
               false, Transparency.OPAQUE, DataBuffer.TYPE_BYTE);
         SampleModel sm = colorModel.createCompatibleSampleModel (width, height);
         DataBufferByte db = new DataBufferByte (buffer, width * height);
         WritableRaster wr = Raster.createWritableRaster (sm, db, null);
         bufferedImage = new BufferedImage (colorModel, wr, false, null);
      }

      return bufferedImage;
   }

   @Override
   public void dispose ()
   {
      super.dispose ();
      numImages = 1;
      codeStreams.clear ();
   }

   @Override
   public void reset ()
   {
      super.setInput (null, false, false);
      dispose ();
      numImages = -1;
   }

   @Override
   public int getTileWidth (int imageIndex) throws IOException
   {
      checkImageIndex (imageIndex);
      int tileWidth = codeStreams.get (imageIndex).getTileWidth ();
      LOGGER.info ("tile width: " + tileWidth);
      return tileWidth;
   }

   @Override
   public int getTileHeight (int imageIndex) throws IOException
   {
      checkImageIndex (imageIndex);
      int tileHeight = codeStreams.get (imageIndex).getTileHeight ();
      LOGGER.info ("tile height: " + tileHeight);
      return tileHeight;
   }

   @Override
   public void setInput (Object input, boolean seekForwardOnly,
         boolean ignoreMetadata)
   {
      reset ();
      if (input == null)
      {
         throw new NullPointerException ("The provided input is null");
      }

      if (input instanceof File)
      {
         inputFile = (File) input;
         filePath = inputFile.getAbsolutePath ();
      }
      else if (input instanceof byte[])
      {
         decoder.setByteInputStream ((byte[]) input);
      }
      else if (input instanceof ImageInputStream)
      {
         try
         {
            ImageInputStream iis = (ImageInputStream) input;
            decoder.setByteInputStream (
                  Utils.imageInputStreamToByteArray (iis));
         }
         catch (IOException e)
         {
            throw new RuntimeException ("Unable to read input data", e);
         }
      }
      else
      {
         throw new IllegalArgumentException ("Invalid input type.");
      }

      numImages = 1;
      super.setInput (input, seekForwardOnly, ignoreMetadata);
   }

   @Override
   public ImageReadParam getDefaultReadParam ()
   {
      // TODO OpenJpegDefaultImageReadParam class
      throw new UnsupportedOperationException ("Not yet implemented");
   }

   public int getSourceDWTLevels (int imageIndex)
   {
      checkImageIndex (imageIndex);
      return codeStreams.get (imageIndex).getSourceDWTLevels ();
   }

   /**
    * Checks existing of image index.
    *
    * @param imageIndex image index.
    */
   private void checkImageIndex (final int imageIndex)
   {
      try
      {
         if (getNumImages (true) == -1)
         {
            throw new IllegalStateException ("Invalid image");
         }

         if (imageIndex < 0 || imageIndex > getNumImages (true))
         {
            StringBuilder sb = new StringBuilder ("Invalid image index ");
            sb.append (imageIndex)
                  .append (" range should be [0, ")
                  .append (numImages).append ('[');

            throw new IndexOutOfBoundsException (sb.toString ());
         }
      }
      catch (IOException e)
      {
         LOGGER.error ("image does not have index", e);
         throw new IllegalStateException ("Invalid image");
      }
   }

}
