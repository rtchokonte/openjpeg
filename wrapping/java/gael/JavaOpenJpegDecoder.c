#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <jni.h>
#include <jni_md.h>

#include "openjpeg.h"
#include "opj_includes.h"
#include "opj_getopt.h"
#include "convert.h"
#include "dirent.h"
#include "format_defs.h"
#include "color.h"
#include "fr_gael_openjpeg_OpenJpegDecoder.h"

/******************************************************************************/
/*** STRUCTURES ***************************************************************/
typedef enum opj_prec_mode
{
   OPJ_PREC_MODE_CLIP,
   OPJ_PREC_MODE_SCALE
}opj_precision_mode;

typedef struct opj_prec
{
   OPJ_UINT32         prec;
   opj_precision_mode mode;
}opj_precision;

typedef struct opj_decompress_params
{
   /** core library parameters */
   opj_dparameters_t core;

   /** input file name */
   char infile[OPJ_PATH_LEN];
   /** output file name */
   char outfile[OPJ_PATH_LEN];
   /** input file format 0: J2K, 1: JP2, 2: JPT */
   int decod_format;
   /** output file format 0: PGX, 1: PxM, 2: BMP */
   int cod_format;
   /** index file name */
   char indexfilename[OPJ_PATH_LEN];

   /** Decoding area left boundary */
   OPJ_UINT32 DA_x0;
   /** Decoding area right boundary */
   OPJ_UINT32 DA_x1;
   /** Decoding area up boundary */
   OPJ_UINT32 DA_y0;
   /** Decoding area bottom boundary */
   OPJ_UINT32 DA_y1;
   /** Verbose mode */
   OPJ_BOOL m_verbose;

   /** tile number ot the decoded tile*/
   OPJ_UINT32 tile_index;
   /** Nb of tile to decode */
   OPJ_UINT32 nb_tile_to_decode;

   opj_precision* precision;
   OPJ_UINT32     nb_precision;

   /* force output colorspace to RGB */
   int force_rgb;
   /* upsample components according to their dx/dy values */
   int upsample;
   /* split output components to different files */
   int split_pnm;
}opj_decompress_parameters;

typedef struct input_stream
{
   // stream content
   const unsigned char* stream;
   // stream length
   size_t length;
   // current index of stream
   size_t index;
}input_stream_t;

/******************************************************************************/
/*** FUNCTIONS ****************************************************************/
static jclass get_object_class (JNIEnv* env, jobject obj)
{
   return (*env)->GetObjectClass(env, obj);
}

input_stream_t* create_input_stream(const unsigned char* stream, size_t length)
{
   input_stream_t* is = NULL;

   is = (input_stream_t*) opj_calloc(1, sizeof(input_stream_t));
   if (!is)
   {
      return 00;
   }
   is->stream = stream;
   is->length = length;
   is->index = 0;

   return is;
}

void free_input_stream(input_stream_t* i_stream)
{
   opj_free(i_stream);
}

OPJ_SIZE_T read_input_stream(void * p_buffer, OPJ_SIZE_T p_nb_bytes, void * p_user_data)
{
   input_stream_t* is = (input_stream_t*) p_user_data;
   OPJ_SIZE_T max_bytes_read;
   OPJ_SIZE_T new_index;
   
   if (is->index >= is->length)
      return ((OPJ_SIZE_T) -1);
   
   new_index = is->index + p_nb_bytes;
   if (new_index > is->length)
      max_bytes_read = is->length - is->index;
   else
      max_bytes_read = p_nb_bytes;
   
   
   void* stream_start = (void*)is->stream;
   void* stream_cursor = (void*)(stream_start + is->index);
   
   memcpy(p_buffer, stream_cursor, max_bytes_read);
   is->index = is->index + max_bytes_read;
   return max_bytes_read;
}

OPJ_SIZE_T write_input_stream(void * p_buffer, OPJ_SIZE_T p_nb_bytes, void * p_user_data)
{
   input_stream_t* is = (input_stream_t*) p_user_data;
   OPJ_SIZE_T max_bytes_write;
   OPJ_SIZE_T new_index;
   
   if (is->index >= is->length)
      return ((OPJ_SIZE_T) -1);
   
   new_index = is->index + p_nb_bytes;
   if (new_index > is->length)
      max_bytes_write = is->length - is->index;
   else
      max_bytes_write = p_nb_bytes;
   
   memcpy((((void*)is->stream)+is->index), p_buffer, max_bytes_write);
   is->index = is->index + max_bytes_write;
   return max_bytes_write;
}

OPJ_OFF_T skip_input_steam(OPJ_OFF_T p_nb_bytes, void * p_user_data)
{
   input_stream_t* is = (input_stream_t*) p_user_data;
   OPJ_SIZE_T new_index = is->index + p_nb_bytes;
   
   if (new_index > is->length)
   {
      is->index = is->length;
      return ((OPJ_OFF_T) -1);
   }
   is->index = is->index + p_nb_bytes;
   return p_nb_bytes;
}

OPJ_BOOL seek_input_stream(OPJ_OFF_T p_nb_bytes, void * p_user_data)
{
   input_stream_t* is = (input_stream_t*) p_user_data;
   
   if (p_nb_bytes > is->length)
   {
      is->index = is->length;
      return OPJ_FALSE;
   }
   
   is->index = p_nb_bytes;
   return OPJ_TRUE;
}

static opj_stream_t* create_opj_input_stream(const unsigned char* content, size_t length)
{
   opj_stream_t* l_stream = 00;
   input_stream_t* i_stream = 00;
   
   i_stream = create_input_stream(content, length);
   if (!i_stream)
   {
      return NULL;
   }
   
   l_stream = opj_stream_create(OPJ_J2K_STREAM_CHUNK_SIZE, OPJ_TRUE);
   if (!l_stream)
   {
      free_input_stream(i_stream);
      return NULL;
   }

   opj_stream_set_user_data(l_stream, i_stream, (opj_stream_free_user_data_fn) free_input_stream);
   opj_stream_set_user_data_length(l_stream, (OPJ_UINT64)length);
   opj_stream_set_read_function(l_stream, (opj_stream_read_fn) read_input_stream);
   opj_stream_set_write_function(l_stream, (opj_stream_write_fn) write_input_stream);
   opj_stream_set_skip_function(l_stream, (opj_stream_skip_fn) skip_input_steam);
   opj_stream_set_seek_function(l_stream, (opj_stream_seek_fn) seek_input_stream);
   
   return (opj_stream_t*) l_stream;
}

void stream_destroy(opj_stream_t* p_stream)
{
   opj_stream_private_t* l_stream = (opj_stream_private_t*) p_stream;

   if (l_stream)
   {
      if (l_stream->m_free_user_data_fn)
         l_stream->m_free_user_data_fn(l_stream->m_user_data);

      l_stream->m_stored_data = 00;
      opj_free(l_stream);
   }
}

static void set_default_parameters(opj_decompress_parameters* parameters)
{
   if (parameters)
   {
      memset(parameters, 0, sizeof(opj_decompress_parameters));
      parameters->decod_format = -1;
      parameters->cod_format = -1;
      opj_set_default_decoder_parameters(&(parameters->core));
   }
}

static void destroy_parameters(opj_decompress_parameters* parameters)
{
   if (parameters)
   {
      if (parameters->precision)
      {
         opj_free(parameters->precision);
         parameters->precision = NULL;
      }
   }
}

void my_error_callback(const char* msg, void* data)
{
   fprintf(stderr, "%s", msg);
}


static void set_image_properties(JNIEnv* env, jclass class, jobject obj, opj_image_t* image)
{
   jmethodID method_id;

   // set width
   method_id = (*env)->GetMethodID(env, class, "setWidth", "(I)V");
   (*env)->CallVoidMethod(env, obj, method_id, image->comps->w);

   // set height
   method_id = (*env)->GetMethodID(env, class, "setHeight", "(I)V");
   (*env)->CallVoidMethod(env, obj, method_id, image->comps->h);

   // set precision
   method_id = (*env)->GetMethodID(env, class, "setPrecision", "(I)V");
   (*env)->CallVoidMethod(env, obj, method_id, image->comps->prec);
   
   // set composition number
   method_id = (*env)->GetMethodID(env, class, "setComponentsNumber", "(I)V");
   (*env)->CallVoidMethod(env, obj, method_id, image->numcomps);
   
}

static void fill_image_24_java_buffer(JNIEnv* env, jobject obj, opj_image_t* image)
{
   size_t length;
   int i;
   int *alpha = NULL, *red = NULL, *green = NULL, *blue = NULL;
   unsigned char rc, gc, bc, ac = 255; // ac:0 FULLY_TRANSPARENT / ac:255 FULLY_OPAQUE
   jboolean has_alpha = (image->numcomps == 4) ? JNI_TRUE : JNI_FALSE;
   jboolean is_copy = JNI_FALSE;
   jintArray image_obj;
   jint* buffer;
   jclass class = get_object_class(env, obj);
   jfieldID fid;
   jmethodID method_id;

   red = image->comps[0].data;
   green = image->comps[1].data;
   blue = image->comps[2].data;
   if (has_alpha == JNI_TRUE)
      alpha = image->comps[3].data;

   // allocate java memory
   method_id = (*env)->GetMethodID(env, class, "alloc24", "()V");
   (*env)->CallVoidMethod(env, obj, method_id);

   // get Java image buffer
   fid = (*env)->GetFieldID(env, class, "image24", "[I");
   image_obj = (*env)->GetObjectField(env, obj, fid);
   length = (*env)->GetArrayLength(env, image_obj);
   buffer = (*env)->GetIntArrayElements(env, image_obj, &is_copy);

   // fill Java image buffer
   for(i = 0; i < length; i++)
   {
      rc = (unsigned char) *red++;
      gc = (unsigned char) *green++;
      bc = (unsigned char) *blue++;
      if (has_alpha == JNI_TRUE)
         ac = (unsigned char) *alpha++;

      *buffer++ = (jint) ((ac << 24) | (rc << 16) | (gc << 8) | bc);
   }

   // release Java image buffer
   (*env)->ReleaseIntArrayElements(env, image_obj, buffer, 0);
}

static void fill_image_16_java_buffer(JNIEnv* env, jobject obj, opj_image_t* image)
{
   size_t length;
   int i;
   int* grey_color;
   jboolean is_copy = JNI_FALSE;
   jshortArray image_obj;
   jshort* buffer;
   jclass class = get_object_class(env, obj);
   jfieldID fid;
   jmethodID method_id;

   // safe pointer copy
   grey_color = image->comps[0].data;

   // allocate java memory
   method_id = (*env)->GetMethodID(env, class, "alloc16", "()V");
   (*env)->CallVoidMethod(env, obj, method_id);

   // get Java image buffer
   fid = (*env)->GetFieldID(env, class, "image16", "[S");
   image_obj = (*env)->GetObjectField(env, obj, fid);
   length = (*env)->GetArrayLength(env, image_obj);
   buffer = (*env)->GetShortArrayElements(env, image_obj, &is_copy);

   // fill Java image buffer
   for(i = 0; i < length; i++)
   {
      *buffer++ = *grey_color++;
   }

   // release Java image buffer
   (*env)->ReleaseShortArrayElements(env, image_obj, buffer, 0);
}

static void fill_image_8_java_buffer(JNIEnv* env, jobject obj, opj_image_t* image)
{
   size_t length;
   int i;
   int* grey_color;
   jboolean is_copy = JNI_FALSE;
   jbyteArray image_obj;
   jbyte* buffer;
   jclass class = get_object_class(env, obj);
   jfieldID fid;
   jmethodID method_id;

   // safe pointer copy
   grey_color = image->comps[0].data;

   // allocate java memory
   method_id = (*env)->GetMethodID(env, class, "alloc8", "()V");
   (*env)->CallVoidMethod(env, obj, method_id);

   // get Java image buffer
   fid = (*env)->GetFieldID(env, class, "image8", "[B");
   image_obj = (*env)->GetObjectField(env, obj, fid);
   length = (*env)->GetArrayLength(env, image_obj);
   buffer = (*env)->GetByteArrayElements(env, image_obj, &is_copy);

   // fill Java image buffer
   for(i = 0; i < length; i++)
   {
      *buffer++ = *grey_color++;
   }

   // release Java image buffer
   (*env)->ReleaseByteArrayElements(env, image_obj, buffer, 0);
}

/******************************************************************************/
/*** LOG FUNCTIONS ************************************************************/
static jobject get_logger_from_object(JNIEnv* env,jobject obj)
{
   jclass class = get_object_class(env, obj);
   jfieldID fid = (*env)->GetFieldID(env, class, "logger", "Lorg/apache/log4j/Logger;");
   
   if ((*env)->ExceptionOccurred(env) || fid == NULL)
      return NULL;
   else
      return (*env)->GetObjectField(env, obj, fid);
}

static void java_log_info (JNIEnv* env, jobject obj, char* message)
{
   jobject logger_obj = get_logger_from_object(env, obj);
   if (logger_obj == NULL) return;

   jclass logger_class = get_object_class(env, logger_obj);
   jmethodID method_id = (*env)->GetMethodID(env, logger_class, "info", "(Ljava/lang/Object;)V");
   if ((*env)->ExceptionOccurred(env) || method_id == NULL) return;

   jstring string = (*env)->NewStringUTF(env, message);
   (*env)->CallVoidMethod(env, logger_obj, method_id, string);
   (*env)->DeleteLocalRef(env, string);
   
}

static void java_log_warn (JNIEnv* env, jobject obj, char* message)
{
   jobject logger_obj = get_logger_from_object(env, obj);
   if (logger_obj == NULL) return;

   jclass logger_class = get_object_class(env, logger_obj);
   jmethodID method_id = (*env)->GetMethodID(env, logger_class, "warn", "(Ljava/lang/Object;)V");
   if ((*env)->ExceptionOccurred(env) || method_id == NULL) return;

   jstring string = (*env)->NewStringUTF(env, message);
   (*env)->CallVoidMethod(env, logger_obj, method_id, string);
   (*env)->DeleteLocalRef(env, string);
}

static void java_log_error (JNIEnv* env, jobject obj, char* message)
{
   jobject logger_obj = get_logger_from_object(env, obj);
   if (logger_obj == NULL) return;

   jclass logger_class = get_object_class(env, logger_obj);
   jmethodID method_id = (*env)->GetMethodID(env, logger_class, "error", "(Ljava/lang/Object;)V");
   if ((*env)->ExceptionOccurred(env) || method_id == NULL) return;

   jstring string = (*env)->NewStringUTF(env, message);
   (*env)->CallVoidMethod(env, logger_obj, method_id, string);
   (*env)->DeleteLocalRef(env, string);
}

/******************************************************************************/
/***JNI CALL ******************************************************************/
JNIEXPORT jboolean JNICALL Java_fr_gael_openjpeg_OpenJpegDecoder_internalOpenJpegDecodeHeader
  (JNIEnv* env, jobject obj)
{
   jclass class = get_object_class(env, obj);
   jfieldID fid = NULL;
   jbyteArray array_source;
   jsize array_size;
   jbyte* source = NULL;
   jboolean is_copy = JNI_FALSE;

   opj_codec_t* codec = NULL;
   opj_stream_t* stream_source = NULL;
   opj_decompress_parameters params;
   opj_image_t* image = NULL;

   // init decoder parameters
   set_default_parameters (&params);
   opj_reset_options_reading();
   params.decod_format = OPJ_CODEC_JP2; // TODO generic for all OPJ_CODEC_FORMAT
   codec = opj_create_decompress(OPJ_CODEC_JP2);
   if (opj_setup_decoder(codec, &(params.core)) == OPJ_FALSE)
   {
      opj_destroy_codec(codec);
      destroy_parameters(&params);
      java_log_error(env, obj, "Decoder setup faild !");
      return JNI_FALSE;
   }
   
   // comment it to disable verbose mode
 //  opj_set_info_handler(codec, my_error_callback, NULL);
 //  opj_set_warning_handler(codec, my_error_callback, NULL);
 //  opj_set_error_handler(codec, my_error_callback, NULL);

   // create stream
   fid = (*env)->GetFieldID(env, class, "byteInputStream", "[B");
   if((*env)->ExceptionOccurred(env) || fid == NULL)
   {
      opj_destroy_codec(codec);
      destroy_parameters(&params);
      return JNI_FALSE;
   }
   array_source = (*env)->GetObjectField(env, obj, fid);
   array_size = (*env)->GetArrayLength(env, array_source);
   source = (*env)->GetByteArrayElements(env, array_source, &is_copy);
   stream_source = create_opj_input_stream((unsigned char*)source, (size_t) array_size);
   if (stream_source == NULL)
   {
      (*env)->ReleaseByteArrayElements(env, array_source, source, JNI_ABORT);
      opj_destroy_codec(codec);
      destroy_parameters(&params);
      java_log_error(env, obj, "Native stream generation faild !");
      return JNI_FALSE;
   }
   
   // Read the main header of the codestream and if necessary the JP2 boxes
   if (opj_read_header(stream_source, codec, &image) == OPJ_FALSE)
   {
      opj_image_destroy(image);
      stream_destroy(stream_source);
      (*env)->ReleaseByteArrayElements(env, array_source, source, JNI_ABORT);
      opj_destroy_codec(codec);
      destroy_parameters(&params);
      java_log_error(env, obj, "Can not read JP2 header !");
      return JNI_FALSE;
   }
   
   // free decode resources
   stream_destroy(stream_source);
   (*env)->ReleaseByteArrayElements(env, array_source, source, 0);
   opj_destroy_codec(codec);
   destroy_parameters(&params);

   set_image_properties(env, class, obj, image);

   // free image resource
   opj_image_destroy(image);   
   
   return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_fr_gael_openjpeg_OpenJpegDecoder_internalOpenJpegDecode
  (JNIEnv* env, jobject obj)
{
   jclass class = get_object_class(env, obj);
   jfieldID fid = NULL;
   jbyteArray array_source;
   jsize array_size;
   jbyte* source = NULL;
   jboolean is_copy = JNI_FALSE;

   opj_codec_t* codec = NULL;
   opj_stream_t* stream_source = NULL;
   opj_decompress_parameters params;
   opj_image_t* image = NULL;

   // init decoder parameters
   set_default_parameters (&params);
   opj_reset_options_reading();
   params.decod_format = OPJ_CODEC_JP2; // TODO generic for all OPJ_CODEC_FORMAT
   codec = opj_create_decompress(OPJ_CODEC_JP2);
   if (opj_setup_decoder(codec, &(params.core)) == OPJ_FALSE)
   {
      opj_destroy_codec(codec);
      destroy_parameters(&params);
      java_log_error(env, obj, "Decoder setup faild !");
      return JNI_FALSE;
   }
   
   // comment it to disable verbose mode
 //  opj_set_info_handler(codec, my_error_callback, NULL);
 //  opj_set_warning_handler(codec, my_error_callback, NULL);
 //  opj_set_error_handler(codec, my_error_callback, NULL);

   // create stream
   fid = (*env)->GetFieldID(env, class, "byteInputStream", "[B");
   if((*env)->ExceptionOccurred(env) || fid == NULL)
   {
      opj_destroy_codec(codec);
      destroy_parameters(&params);
      return JNI_FALSE;
   }
   array_source = (*env)->GetObjectField(env, obj, fid);
   array_size = (*env)->GetArrayLength(env, array_source);
   source = (*env)->GetByteArrayElements(env, array_source, &is_copy);
   stream_source = create_opj_input_stream((unsigned char*)source, (size_t) array_size);
   if (stream_source == NULL)
   {
      (*env)->ReleaseByteArrayElements(env, array_source, source, JNI_ABORT);
      opj_destroy_codec(codec);
      destroy_parameters(&params);
      java_log_error(env, obj, "Native stream generation faild !");
      return JNI_FALSE;
   }
   
   // Read the main header of the codestream and if necessary the JP2 boxes
   if (opj_read_header(stream_source, codec, &image) == OPJ_FALSE)
   {
      opj_image_destroy(image);
      stream_destroy(stream_source);
      (*env)->ReleaseByteArrayElements(env, array_source, source, JNI_ABORT);
      opj_destroy_codec(codec);
      destroy_parameters(&params);
      java_log_error(env, obj, "Can not read JP2 header !");
      return JNI_FALSE;
   }
   
   // decode image
   if (!opj_decode(codec, stream_source, image) && opj_end_decompress(codec, stream_source))
   {
      opj_image_destroy(image);
      stream_destroy(stream_source);
      (*env)->ReleaseByteArrayElements(env, array_source, source, JNI_ABORT);
      opj_destroy_codec(codec);
      destroy_parameters(&params);
      java_log_error(env, obj, "Can not decode JP2 !");
      return JNI_FALSE;
   }
   
   // free decode resources
   stream_destroy(stream_source);
   (*env)->ReleaseByteArrayElements(env, array_source, source, 0);
   opj_destroy_codec(codec);
   destroy_parameters(&params);
   
   // check YUV color space format
   if (image->color_space != OPJ_CLRSPC_SYCC && image->numcomps == 3 &&
         image->comps[0].dx == image->comps[0].dy && image->comps[1].dx != 1)
   {
      image->color_space = OPJ_CLRSPC_SYCC;
   }
   else if (image->color_space <= 2) // check GRAY scale color space
   {
      image->color_space = OPJ_CLRSPC_GRAY;
   }
   
   // convert YUV color space to RGB color space
   if (image->color_space == OPJ_CLRSPC_SYCC)
   {
      color_sycc_to_rgb(image);
   }
   
   // active icc profile
   if (image->icc_profile_buf)
   {
#if defined(HAVE_LIBLCMS1) || defined(HAVE_LIBLCMS2)
      color_apply_icc_profile(image);
#endif
      opj_free(image->icc_profile_buf);
      image->icc_profile_buf = NULL;
      image->icc_profile_len = 0;
   }
   
   if (image->numcomps >= 3)
   {
      fill_image_24_java_buffer(env, obj, image);
   }
   else if (image->comps[0].prec > 8)
   {
      fill_image_16_java_buffer (env, obj, image);
   }
   else
   {
      fill_image_8_java_buffer (env, obj, image);
   }

   // free image resource
   opj_image_destroy(image);
   
   return JNI_TRUE;
}
