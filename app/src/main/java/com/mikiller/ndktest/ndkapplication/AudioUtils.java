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
    byte[] tmp;
    ByteBuffer audioBuf;
    AudioRunnable audioRunnable;
    boolean isStart = false;

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

/*    private void saveAudioBuffer() {
//        boolean ret = audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING;
//        if (ret) {
                sample = getAudioData();
                NDKImpl.pushAudio(sample);

//        }
//        return ret;
    }*/

    private byte[] getAudioData() {
        if(audioBuf == null){
            audioBuf = ByteBuffer.allocate(2048 * audioRecord.getChannelCount());
        }else{
            audioBuf.clear();
        }
        if(audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING) {
            int ret = audioRecord.read(audioBuf.array(), 0, 2048 * audioRecord.getChannelCount());
            if (ret < 0) {
                Log.e(CameraActivity.class.getSimpleName(), "get audio failed");
            }
        }else{
            if(tmp == null)
                tmp = new byte[2048 * audioRecord.getChannelCount()];
            audioBuf.put(tmp);
        }
        return audioBuf.array();
    }

    public void start(){
        audioRecord.startRecording();
        isStart = true;
    }

    public void pause(){
        audioRecord.stop();
    }

    public void release(){
        audioRunnable.isLive = false;
        isStart = false;
        audioRecord.stop();
        audioRecord.release();
    }

    private class AudioRunnable implements Runnable {
        public boolean isLive = true;
        byte[] sample;
        @Override
        public void run() {
            while (isLive) {
//                sample = getAudioData();
                if(isStart)
                    NDKImpl.pushAudio(getAudioData());
            }
            isLive = true;
        }
    }
}
