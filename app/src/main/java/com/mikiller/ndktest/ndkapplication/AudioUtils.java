package com.mikiller.ndktest.ndkapplication;

import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.util.Log;

import java.nio.ByteBuffer;

/**
 * Created by Mikiller on 2017/1/23.
 */

public class AudioUtils {
    private static final String TAG = AudioUtils.class.getSimpleName();
    final int AUDIOSOURCE = MediaRecorder.AudioSource.MIC;
    final int SAMPLERATE = 44100;
    AudioRecord audioRecord;
    int audioBufSize = 0;
    byte[] sample;
    ByteBuffer audioBuf;
    AudioRunnable audioRunnable;

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

    public void init(int channelConfig, int audioFormat) {
        audioBufSize = AudioRecord.getMinBufferSize(SAMPLERATE, channelConfig, audioFormat);
        if (audioBufSize == AudioRecord.ERROR_BAD_VALUE) {
            Log.e(CameraActivity.class.getSimpleName(), "audio param is wrong");
            return;
        }
        audioRecord = new AudioRecord(AUDIOSOURCE, SAMPLERATE, channelConfig, audioFormat, audioBufSize);
        if (audioRecord.getState() == AudioRecord.STATE_UNINITIALIZED) {
            Log.e(CameraActivity.class.getSimpleName(), "init audio failed");
            return;
        }
//        audioRecord.startRecording();
        audioRunnable = new AudioRunnable();
    }

    private void saveAudioBuffer(boolean isPause) {
//        boolean ret = audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING;
//        if (ret) {
                sample = getAudioData();
                NDKImpl.pushAudio(sample);

//        }
//        return ret;
    }

    private byte[] getAudioData() {
        if(audioBuf == null){
            audioBuf = ByteBuffer.allocate(2048 * audioRecord.getChannelCount());
        }else{
            audioBuf.clear();
        }

        int ret = audioRecord.read(audioBuf.array(), 0, 2048*audioRecord.getChannelCount());
        if(ret < 0){
            Log.e(CameraActivity.class.getSimpleName(), "get audio failed");
        }
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

    public void start(){
        audioRecord.startRecording();
        audioRunnable.isPause = false;
    }

    public void pause(){
        audioRunnable.isPause = true;
        audioRecord.stop();
    }

    public void release(){
        audioRunnable.isLive = false;
        audioRecord.stop();
        audioRecord.release();
    }

    private class AudioRunnable implements Runnable {
        public boolean isLive = true, isPause = true;
        @Override
        public void run() {
            while (isLive) {
//                if (!isPause)
                    saveAudioBuffer(isPause);
            }
            isLive = true;
            isPause = true;
        }
    }
}
