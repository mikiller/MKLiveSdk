package com.mikiller.ndktest.ndkapplication;

import android.annotation.SuppressLint;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.media.Image;
import android.media.ImageReader;
import android.util.Log;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;

/**
 * Created by Mikiller on 2016/12/6.
 */

public class YUVUtils {
    private static final String TAG = YUVUtils.class.getSimpleName();

    public static final int COLOR_FormatI420 = 1;
    public static final int COLOR_FormatNV21 = 2;

    @SuppressLint("NewApi")
    public static byte[] getDataFromImage(Image image, int colorFormat) {
        if (colorFormat != COLOR_FormatI420 && colorFormat != COLOR_FormatNV21) {
            throw new IllegalArgumentException("only support COLOR_FormatI420 and COLOR_FormatNV21");
        }
        if (!isImageFormatSupported(image)) {
            throw new RuntimeException("can't convert Image to byte array, format " + image.getFormat());
        }
        Rect crop = image.getCropRect();
        int format = image.getFormat();
        int width = crop.width();
        int height = crop.height();
        Image.Plane[] planes = image.getPlanes();
        byte[] data = new byte[width * height * ImageFormat.getBitsPerPixel(format) / 8];
        byte[] rowData = new byte[planes[0].getRowStride()];
        Log.v(CameraActivity.class.getSimpleName(), "get data from " + planes.length + " planes");
        int channelOffset = 0;
        int outputStride = 1;
        for (int i = 0; i < planes.length; i++) {
//        int i = 0;
            switch (i) {
                case 0:
                    channelOffset = 0;
                    outputStride = 1;
                    break;
                case 1:
                    if (colorFormat == COLOR_FormatI420) {
                        channelOffset = width * height;
                        outputStride = 1;
                    } else if (colorFormat == COLOR_FormatNV21) {
                        channelOffset = width * height + 1;
                        outputStride = 2;
                    }
                    break;
                case 2:
                    if (colorFormat == COLOR_FormatI420) {
                        channelOffset = (int) (width * height * 1.25);
                        outputStride = 1;
                    } else if (colorFormat == COLOR_FormatNV21) {
                        channelOffset = width * height;
                        outputStride = 2;
                    }
                    break;
            }
            ByteBuffer buffer = planes[i].getBuffer();
            int rowStride = planes[i].getRowStride();
            int pixelStride = planes[i].getPixelStride();
//            Log.v(CameraActivity.class.getSimpleName(), "pixelStride " + pixelStride);
//            Log.v(CameraActivity.class.getSimpleName(), "rowStride " + rowStride);
//            Log.v(CameraActivity.class.getSimpleName(), "width " + width);
//            Log.v(CameraActivity.class.getSimpleName(), "height " + height);
//            Log.v(CameraActivity.class.getSimpleName(), "buffer size " + buffer.remaining());

            int shift = (i == 0) ? 0 : 1;
            int w = width >> shift;
            int h = height >> shift;
            int pos = rowStride * (crop.top >> shift) + pixelStride * (crop.left >> shift);
            buffer.position(pos);
            for (int row = 0; row < h; row++) {
                int length;
                if (pixelStride == 1 && outputStride == 1) {
                    length = w;
//                    Log.e(TAG, String.format("pos: %d", buffer.position()));
//                    Log.e(TAG, String.format("offset: %d", channelOffset));
                    buffer.get(data, channelOffset, length);
                    channelOffset += length;
//                    Log.e(TAG, String.format("new pos: %d", buffer.position()));
                } else {
                    length = (w - 1) * pixelStride + 1;
                    if(row > h - 3) {
                        Log.e(TAG, "length: " + length);
                        Log.e(TAG, String.format("pos: %d", buffer.position()));
                        Log.e(TAG, "w: " + w + "pixel: " + pixelStride);
                    }
                    buffer.get(rowData, 0, length);

                    for (int col = 0; col < w; col++) {
                        data[channelOffset] = rowData[col * pixelStride];
                        channelOffset += outputStride;
                    }
                }
                if (row < h - 1) {
//                    if(outputStride == 2){
//                        Log.e(TAG, String.format("pos: %d", buffer.position()));
//                    }
                    buffer.position(buffer.position() + rowStride - length);
//                    if(outputStride == 2){
//                        Log.e(TAG, String.format("new pos: %d", buffer.position()));
//                    }
//                    if(row < 4)
//                        Log.e(TAG, "buffer new pos:" + buffer.position());
                }
            }
//            Log.v(CameraActivity.class.getSimpleName(), "Finished reading data from plane " + i);
        }
        return data;
    }

    @SuppressLint("NewApi")
    private static boolean isImageFormatSupported(Image image) {
        int format = image.getFormat();
        switch (format) {
            case ImageFormat.YUV_420_888:
            case ImageFormat.NV21:
            case ImageFormat.YV12:
                return true;
        }
        return false;
    }
    @SuppressLint("NewApi")
    public static ByteBuffer convertYUV420ToN21(Image imgYUV420, boolean grayscale) {

        Image.Plane yPlane = imgYUV420.getPlanes()[0];
        byte[] yData = getRawCopy(yPlane.getBuffer());

        Image.Plane uPlane = imgYUV420.getPlanes()[1];
        byte[] uData = getRawCopy(uPlane.getBuffer());

        Image.Plane vPlane = imgYUV420.getPlanes()[2];
        byte[] vData = getRawCopy(vPlane.getBuffer());

        // NV21 stores a full frame luma (y) and half frame chroma (u,v), so total size is
        // size(y) + size(y) / 2 + size(y) / 2 = size(y) + size(y) / 2 * 2 = size(y) + size(y) = 2 * size(y)
        int npix = imgYUV420.getWidth() * imgYUV420.getHeight();
        byte[] nv21Image = new byte[npix * 2];
        Arrays.fill(nv21Image, (byte)127); // 127 -> 0 chroma (luma will be overwritten in either case)

        // Copy the Y-plane
        ByteBuffer nv21Buffer = ByteBuffer.wrap(nv21Image);
        for(int i = 0; i < imgYUV420.getHeight(); i++) {
            nv21Buffer.put(yData, i * yPlane.getRowStride(), imgYUV420.getWidth());
        }

        // Copy the u and v planes interlaced
        if(!grayscale) {
            for (int row = 0; row < imgYUV420.getHeight() / 2; row++) {
                for (int cnt = 0, upix = 0, vpix = 0; cnt < imgYUV420.getWidth() / 2; upix += uPlane.getPixelStride(), vpix += vPlane.getPixelStride(), cnt++) {
                    nv21Buffer.put(uData[row * uPlane.getRowStride() + upix]);
                    nv21Buffer.put(vData[row * vPlane.getRowStride() + vpix]);
                }
            }

            fastReverse(nv21Image, npix, npix);
        }

        fastReverse(nv21Image, 0, npix);

        return nv21Buffer;
    }

    private static byte[] getRawCopy(ByteBuffer in) {
        ByteBuffer rawCopy = ByteBuffer.allocate(in.capacity());
        rawCopy.put(in);
        return rawCopy.array();
    }

    private static void fastReverse(byte[] array, int offset, int length) {
        int end = offset + length;
        for (int i = offset; i < offset + (length / 2); i++) {
            array[i] = (byte)(array[i] ^ array[end - i  - 1]);
            array[end - i  - 1] = (byte)(array[i] ^ array[end - i  - 1]);
            array[i] = (byte)(array[i] ^ array[end - i  - 1]);
        }
    }

    /**
     * Converts the YUV420_888 Image into a packed NV21 of a single byte array,
     * suitable for JPEG compression by the method convertNv21toJpeg. This
     * version will allocate its own byte buffer memory.
     *
     * @param img image to be converted
     * @return byte array of NV21 packed image
     */
    @SuppressLint("NewApi")
    public static byte[] convertYUV420ImageToPackedNV21(Image img) {
        final Image.Plane[] planeList = img.getPlanes();
        ByteBuffer y_buffer = planeList[0].getBuffer();
        ByteBuffer u_buffer = planeList[1].getBuffer();
        ByteBuffer v_buffer = planeList[2].getBuffer();
        byte[] dataCopy = new byte[y_buffer.capacity() + u_buffer.capacity() + v_buffer.capacity()];
        return convertYUV420ImageToPackedNV21(img, dataCopy);
    }

    /**
     * Converts the YUV420_888 Image into a packed NV21 of a single byte array,
     * suitable for JPEG compression by the method convertNv21toJpeg. Creates a
     * memory block with the y component at the head and interleaves the u,v
     * components following the y component. Caller is responsible to allocate a
     * large enough buffer for results.
     *
     * @param img image to be converted
     * @param dataCopy buffer to write NV21 packed image
     * @return byte array of NV21 packed image
     */
    @SuppressLint("NewApi")
    public static byte[] convertYUV420ImageToPackedNV21(Image img, byte[] dataCopy) {
        // Get all the relevant information and then release the image.
        final int w = img.getWidth();
        final int h = img.getHeight();
        final Image.Plane[] planeList = img.getPlanes();
        ByteBuffer y_buffer = planeList[0].getBuffer();
        ByteBuffer u_buffer = planeList[1].getBuffer();
        ByteBuffer v_buffer = planeList[2].getBuffer();
        final int color_pixel_stride = planeList[1].getPixelStride();
        final int y_size = y_buffer.capacity();
        final int u_size = u_buffer.capacity();
        final int data_offset = w * h;
        for (int i = 0; i < y_size; i++) {
            dataCopy[i] = (byte) (y_buffer.get(i) & 255);
        }
        for (int i = 0; i < u_size / color_pixel_stride; i++) {
            dataCopy[data_offset + 2 * i] = v_buffer.get(i * color_pixel_stride);
            dataCopy[data_offset + 2 * i + 1] = u_buffer.get(i * color_pixel_stride);
        }
        return dataCopy;
    }
}
