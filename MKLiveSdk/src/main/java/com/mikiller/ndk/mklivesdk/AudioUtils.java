package com.mikiller.ndk.mklivesdk;

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

    private AudioUtils() {
    }

    private static class AudioUtilsFactory {
        private static AudioUtils instance = new AudioUtils();
    }

    public static AudioUtils getInstance() {
        return AudioUtilsFactory.instance;
    }

    public void init(int channelConfig, int audioFormat) {
        audioBufSize = AudioRecord.getMinBufferSize(SAMPLERATE, channelConfig, audioFormat);
        if (audioBufSize == AudioRecord.ERROR_BAD_VALUE) {
            Log.e(MKLiveActivity.class.getSimpleName(), "audio param is wrong");
            return;
        }
        audioRecord = new AudioRecord(AUDIOSOURCE, SAMPLERATE, channelConfig, audioFormat, audioBufSize);
        if (audioRecord.getState() == AudioRecord.STATE_UNINITIALIZED) {
            Log.e(MKLiveActivity.class.getSimpleName(), "init audio failed");
            return;
        }
    }

    public byte[] getAudioData() {
        if(audioBuf == null){
            audioBuf = ByteBuffer.allocate(2048 * audioRecord.getChannelCount());
        }else{
            audioBuf.clear();
        }
        if(audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING) {
            int ret = audioRecord.read(audioBuf.array(), 0, 2048 * audioRecord.getChannelCount());
            if (ret < 0) {
                Log.e(MKLiveActivity.class.getSimpleName(), "get audio failed");
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
    }

    public void pause(){
        audioRecord.stop();
    }

    public void release(){
        audioRecord.stop();
        audioRecord.release();
    }
}
