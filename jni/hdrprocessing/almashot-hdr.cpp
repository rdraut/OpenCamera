/*
The contents of this file are subject to the Mozilla Public License
Version 1.1 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
License for the specific language governing rights and limitations
under the License.

The Original Code is collection of files collectively known as Open Camera.

The Initial Developer of the Original Code is Almalence Inc.
Portions created by Initial Developer are Copyright (C) 2013
by Almalence Inc. All Rights Reserved.
*/

#include <stdio.h>
#include <string.h>
#include <jni.h>
#include <android/log.h>

#include "ImageConversionUtils.h"

#include "almashot.h"
#include "hdr.h"

#define MAX_HDR_FRAMES	4


static unsigned char *yuv[MAX_HDR_FRAMES] = {NULL, NULL, NULL, NULL};
static void *instance = NULL;
static int almashot_inited = 0;
static Uint8 *OutPic = NULL;


// This triggers openmp constructors and destructors to be called upon library load/unload
void __attribute__((constructor)) initialize_openmp() {}
void __attribute__((destructor)) release_openmp() {}


extern "C" JNIEXPORT jstring JNICALL Java_com_almalence_plugins_processing_hdr_AlmaShotHDR_Initialize
(
	JNIEnv* env,
	jobject thiz
)
{
	char status[1024];
	int err=0;

	if (almashot_inited == 0)
	{
		err = AlmaShot_Initialize(0);

		if (err == 0)
			almashot_inited = 1;
	}

	sprintf (status, " err: %d\n", err);
	return env->NewStringUTF(status);
}


extern "C" JNIEXPORT jint JNICALL Java_com_almalence_plugins_processing_hdr_AlmaShotHDR_Release
(
	JNIEnv*,
	jobject
)
{
	int i;

	if (almashot_inited == 1)
	{
		AlmaShot_Release();

		almashot_inited = 0;
	}

	return 0;
}


// ToDo: this is a very common operation - use ImageConversion jni interface instead (? - need to avoid global yuv array then?)
extern "C" JNIEXPORT jstring JNICALL Java_com_almalence_plugins_processing_hdr_AlmaShotHDR_HDRConvertFromJpeg
(
	JNIEnv* env,
	jobject thiz,
	jintArray in,
	jintArray in_len,
	jint nFrames,
	jint sx,
	jint sy
)
{
	int i;
	int *jpeg_length;
	unsigned char * *jpeg;
	char status[1024];

	Uint8 *inp[4];
	int x, y;
	int x0_out, y0_out, w_out, h_out;

	jpeg = (unsigned char**)env->GetIntArrayElements(in, NULL);
	jpeg_length = (int*)env->GetIntArrayElements(in_len, NULL);

	DecodeAndRotateMultipleJpegs(yuv, jpeg, jpeg_length, sx, sy, nFrames, 0, 0, 0, true);

	env->ReleaseIntArrayElements(in, (jint*)jpeg, JNI_ABORT);
	env->ReleaseIntArrayElements(in_len, (jint*)jpeg_length, JNI_ABORT);

	//sprintf (status, "frames total: %d\nsize0: %d\nsize1: %d\nsize2: %d\n", (int)nFrames, jpeg_length[0], jpeg_length[1], jpeg_length[2]);
	sprintf (status, "frames total: %d\n", (int)nFrames);
	return env->NewStringUTF(status);
}


extern "C" JNIEXPORT jstring JNICALL Java_com_almalence_plugins_processing_hdr_AlmaShotHDR_HDRPreview
(
	JNIEnv* env,
	jobject thiz,
	jint nFrames,
	jint sx,
	jint sy,
	jintArray jpview,
	jint expoPref,
	jint colorPref,
	jint ctrstPref,
	jint microPref,
	jint noSegmPref,
	jint noisePref,
	jboolean mirror
)
{
	int i;
	int x, y;
	Uint8 *pview_rgb;
	Uint32 *pview;
	int nTable[3] = {1,3,7};

	//__android_log_print(ANDROID_LOG_INFO, "CameraTest", "Preview CALLED %d %d", sx, sy);

	pview = (Uint32 *)env->GetIntArrayElements(jpview, NULL);

	/* debug
	for (i=0; i<nFrames; ++i)
	{
		char s[1024];

		sprintf(s, "/mnt/sdcard/HdrCameraInput/%d.bin", i);
		FILE *f=fopen(s, "wb");
		fwrite (yuv[i], sx*sy*2, 1, f);
		fclose(f);
	}
	*/

	pview_rgb = (Uint8*)malloc((sx/4)*(sy/4)*3);

	if (pview_rgb)
	{
		if (noisePref<0)	// eval version
			Hdr_Preview(&instance, yuv, pview_rgb, NULL, NULL, 256,
				expoPref, colorPref, ctrstPref, microPref, sx, sy, nFrames, 1, noSegmPref, 0, 0, 1, 0);
		else
			Hdr_Preview(&instance, yuv, pview_rgb, NULL, NULL, 256*nTable[noisePref],
				expoPref, colorPref, ctrstPref, microPref, sx, sy, nFrames, 1, noSegmPref, 1, 1, 1, 0);

		AlmaShot_Preview2RGBi(pview_rgb, pview_rgb, sx/4, sy/4, 0, 0, sx/4, sy/4, (sx/4)*3);

		// construct preview in a form suitable for android bitmap
		for (y=0; y<sy/4; ++y)
		{
			int vy = (sy/4)-1-y;

			for (x=0; x<sx/4; ++x)
			{
				int vx;
				if (mirror) vx = (sx/4)-1-x;
					else vx = x;

				pview[vx*(sy/4)+vy] =									// rotate 90 degree for portrait layout
						((Uint32)pview_rgb[(y*(sx/4)+x)*3]<<16) +
						((Uint32)pview_rgb[(y*(sx/4)+x)*3+1]<<8) +
						(Uint32)pview_rgb[(y*(sx/4)+x)*3+2] +
						(255<<24);
			}
		}

		free (pview_rgb);
	}

	env->ReleaseIntArrayElements(jpview, (jint*)pview, JNI_ABORT);

	return env->NewStringUTF("ok");
}


extern "C" JNIEXPORT jstring JNICALL Java_com_almalence_plugins_processing_hdr_AlmaShotHDR_HDRPreview2
(
	JNIEnv* env,
	jobject thiz,
	jint sx,
	jint sy,
	jintArray jpview,
	jboolean mirror
)
{
	int i;
	int x, y;
	Uint8 *pview_rgb;
	Uint32 *pview;

	//__android_log_print(ANDROID_LOG_INFO, "CameraTest", "Preview2 CALLED %d %d", sx, sy);

	pview = (Uint32 *)env->GetIntArrayElements(jpview, NULL);

	pview_rgb = (Uint8*)malloc((sx/4)*(sy/4)*3);

	if (pview_rgb)
	{
		Hdr_Preview2(instance, pview_rgb, 0,0,0,0,0);

		AlmaShot_Preview2RGBi(pview_rgb, pview_rgb, sx/4, sy/4, 0, 0, sx/4, sy/4, (sx/4)*3);

		// construct preview in a form suitable for android bitmap
		for (y=0; y<sy/4; ++y)
		{
			int vy = (sy/4)-1-y;

			for (x=0; x<sx/4; ++x)
			{
				int vx;
				if (mirror) vx = (sx/4)-1-x;
					else vx = x;

				pview[vx*(sy/4)+vy] =									// rotate 90 degree for portrait layout
						((Uint32)pview_rgb[(y*(sx/4)+x)*3]<<16) +
						((Uint32)pview_rgb[(y*(sx/4)+x)*3+1]<<8) +
						(Uint32)pview_rgb[(y*(sx/4)+x)*3+2] +
						(255<<24);
			}
		}

		free (pview_rgb);
	}

	/* debug
	{
		FILE *fd1;
		int ch;

		fd1 = fopen("/mnt/sdcard/HdrCameraInput/map.bmp","wb");
		fwrite(header,54,1,fd1);

		// write output data
		for (y=sy-1;y>=0;--y)				// bmp images are stored bottom-up
			for (x=0;x<sx;++x)
				for (ch=2;ch>=0;--ch)		// bmp colors are stored BGR
				{
					fwrite(&debug[(x+y*sx)*3+ch],1,1,fd1);
				}

		fclose(fd1);
	}
	*/

	env->ReleaseIntArrayElements(jpview, (jint*)pview, JNI_ABORT);

	return env->NewStringUTF("ok");
}


extern "C" JNIEXPORT jstring JNICALL Java_com_almalence_plugins_processing_hdr_AlmaShotHDR_HDRPreview2a
(
	JNIEnv* env,
	jobject thiz,
	jint sx,
	jint sy,
	jintArray jpview,
	jboolean jrot,
	jint exposure,
	jint vividness,
	jint contrast,
	jint microcontrast,
	jboolean mirror
)
{
	int i;
	int x, y;
	Uint8 *pview_rgb;
	Uint32 *pview;

	//__android_log_print(ANDROID_LOG_INFO, "CameraTest", "Preview2a CALLED %d %d", sx, sy);

	pview = (Uint32 *)env->GetIntArrayElements(jpview, NULL);

	pview_rgb = (Uint8*)malloc((sx/4)*(sy/4)*3);

	if (pview_rgb)
	{
		Hdr_Preview2(instance, pview_rgb, 1, exposure, vividness, contrast, microcontrast);

		AlmaShot_Preview2RGBi(pview_rgb, pview_rgb, sx/4, sy/4, 0, 0, sx/4, sy/4, (sx/4)*3);

		// construct preview in a form suitable for android bitmap
		if (jrot)
		{
			for (y=0; y<sy/4; ++y)
			{
				int vy = (sy/4)-1-y;

				for (x=0; x<sx/4; ++x)
				{
					int vx;
					if (mirror) vx = (sx/4)-1-x;
						else vx = x;

					pview[vx*(sy/4)+vy] =									// rotate 90 degree for portrait layout
							((Uint32)pview_rgb[(y*(sx/4)+x)*3]<<16) +
							((Uint32)pview_rgb[(y*(sx/4)+x)*3+1]<<8) +
							(Uint32)pview_rgb[(y*(sx/4)+x)*3+2] +
							(255<<24);
				}
			}
		}
		else
		{
			for (y=0; y<sy/4; ++y)
			{
				int vy = y;
				//if (mirror) vy = (sy/4)-1-y;

				for (x=0; x<sx/4; ++x)
				{
					int vx = x;
					if (mirror) vx = (sx/4)-1-x;

					pview[vy*(sx/4)+vx] =
							((Uint32)pview_rgb[(y*(sx/4)+x)*3]<<16) +
							((Uint32)pview_rgb[(y*(sx/4)+x)*3+1]<<8) +
							(Uint32)pview_rgb[(y*(sx/4)+x)*3+2] +
							(255<<24);
				}
			}
		}

		free (pview_rgb);
	}

	env->ReleaseIntArrayElements(jpview, (jint*)pview, JNI_ABORT);

	return env->NewStringUTF("ok");
}

extern "C" JNIEXPORT jbyteArray JNICALL Java_com_almalence_plugins_processing_hdr_AlmaShotHDR_HDRProcess
(
	JNIEnv* env,
	jobject thiz,
	jint sx,
	jint sy,
	jintArray jcrop,
	jboolean jrot,
	jboolean mirror
)
{
	Uint8 *OutNV21;
	int *crop;
	int x,y;
	int tmp;
	int allocSize;

	unsigned char *data;
	jbyteArray jdata = env->NewByteArray(0);

	//__android_log_print(ANDROID_LOG_INFO, "CameraTest", "PROCESSING CALLED %d %d", sx, sy);

	if (OutPic)
	{
		//__android_log_print(ANDROID_LOG_INFO, "HDR", "OutPic is not NULL, freeing");
		free(OutPic);
		//__android_log_print(ANDROID_LOG_INFO, "HDR", "OutPic successfuly freed");
	}

	allocSize = sx*sy+(sx+1)*(sy+1)/2;

	OutPic = (Uint8 *)malloc(allocSize);

	crop = (int*)env->GetIntArrayElements(jcrop, NULL);

	//__android_log_print(ANDROID_LOG_INFO, "HDR", "About to call Hdr_Process(%d)", (int)OutPic);
	Hdr_Process(instance, &OutPic, &crop[0], &crop[1], &crop[2], &crop[3], 1);
	//__android_log_print(ANDROID_LOG_INFO, "CameraTest", "Hdr_Process() call returned");

	OutNV21 = OutPic;
	if (jrot)
		OutNV21 = (Uint8 *)malloc(allocSize);

	TransformNV21(OutPic, OutNV21, sx, sy, crop, mirror&&jrot, mirror&&jrot, jrot);

	if (jrot)
	{
		free(OutPic);
		OutPic = OutNV21;
	}

	jdata = env->NewByteArray(allocSize);
	data = (unsigned char*)env->GetByteArrayElements(jdata, NULL);
	memcpy (data, OutPic, allocSize);

	env->ReleaseIntArrayElements(jcrop, (jint*)crop, JNI_ABORT);
	env->ReleaseByteArrayElements(jdata, (jbyte*)data, JNI_ABORT);

	return jdata;
}

extern "C" JNIEXPORT jint JNICALL Java_com_almalence_plugins_processing_hdr_AlmaShotHDR_HDRFreeInstance
(
	JNIEnv*,
	jobject
)
{
	//__android_log_print(ANDROID_LOG_INFO, "HDR", "HDRFreeInstance() called");
	
	if (OutPic)
	{
		//__android_log_print(ANDROID_LOG_INFO, "HDR", "OutPic is not NULL, calling free()");
		free(OutPic);
		//__android_log_print(ANDROID_LOG_INFO, "HDR", "free() returned");
		
		OutPic = NULL;
	}
	
	if (instance)
	{
		//__android_log_print(ANDROID_LOG_INFO, "HDR", "Instance is not NULL, calling Hdr_FreeInstance()");
		Hdr_FreeInstance(instance, 0);
		//__android_log_print(ANDROID_LOG_INFO, "HDR", "Hdr_FreeInstance() returned");
		
		instance = NULL;
	}
}

extern "C" JNIEXPORT jint JNICALL Java_com_almalence_plugins_processing_hdr_AlmaShotHDR_HDRStopProcessing
(
	JNIEnv*,
	jobject
)
{
	//__android_log_print(ANDROID_LOG_INFO, "HDR", "HDRStopProcessing() called");
	Hdr_Cancel(instance);
	//__android_log_print(ANDROID_LOG_INFO, "HDR", "Hdr_Cancel() returned");
}

