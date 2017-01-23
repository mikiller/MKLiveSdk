package com.mikiller.ndktest.ndkapplication;

import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.util.Log;

import java.nio.ByteBuffer;

/**
 * Created by Mikiller on 2017/1/23.
 */

public class AudioUtils {
    private static final String TAG = AudioUtils.class.getSimpleName();

    AudioRecord audioRecord;
    int audioBufSize = 0;
    int audioSource = MediaRecorder.AudioSource.MIC;
    byte[] sample;
    AudioRunnable audioRunnable;
    boolean isRelease;

    private AudioUtils() {
    }

    private static class AudioUtilsFactory {
        private static AudioUtils instance = new AudioUtils();
    }

    public static AudioUtils getInstance() {
        return AudioUtilsFactory.instance;
    }

    public AudioRunnable getAudioRunnable(){
        return audioRunnable;
    }

    public void init(int sampleRate, int channelConfig, int audioFormat) {
        audioBufSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat);
        if (audioBufSize == AudioRecord.ERROR_BAD_VALUE) {
            Log.e(CameraActivity.class.getSimpleName(), "audio param is wrong");
            return;
        }
        audioRecord = new AudioRecord(audioSource, sampleRate, channelConfig, audioFormat, audioBufSize);
        if (audioRecord.getState() == AudioRecord.STATE_UNINITIALIZED) {
            Log.e(CameraActivity.class.getSimpleName(), "init audio failed");
            return;
        }
        audioRunnable = new AudioRunnable();
        isRelease = false;
    }

    private boolean saveAudioBuffer() {
        boolean ret = audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING;
        if (ret) {
            sample = getAudioData();
            NDKImpl.saveAudioBuffer(sample, sample.length);
        }
        return ret;
    }

    private byte[] getAudioData() {
        ByteBuffer audioBuf = ByteBuffer.allocate(audioBufSize);
        int ret = audioRecord.read(audioBuf.array(), 0, audioBuf.capacity());
        if (ret < 0) {
            Log.e(CameraActivity.class.getSimpleName(), "get audio failed");
        } else
            audioBuf.limit(ret);
        return audioBuf.array();
    }

    public void audioSwitch(boolean isPlay) {
        if (audioRecord != null) {
            if (isPlay) {
                audioRecord.startRecording();
                synchronized (audioRunnable) {
                    audioRunnable.notify();
                }
            } else {
                audioRecord.stop();
            }
        }
    }

    public void release(){
        audioRecord.stop();
        audioRecord.release();
        isRelease = true;
    }

    private class AudioRunnable implements Runnable {

        @Override
        public void run() {
            while (!isRelease)
                if (!saveAudioBuffer())
                    synchronized (this) {
                        try {
                            wait();
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }
        }
    }
}
