package com.example.ffmpeg;

import java.io.File;
import java.io.IOException;

import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.app.Activity;
import android.view.Menu;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Toast;

public class MainActivity extends Activity implements OnClickListener {

	static {
		System.loadLibrary("avutil-54");
		System.loadLibrary("avcodec-56");
		System.loadLibrary("avdevice-56");
		System.loadLibrary("avfilter-5");
		System.loadLibrary("avformat-56");
		System.loadLibrary("postproc-53");
		System.loadLibrary("swresample-1");
		System.loadLibrary("swscale-3");
		System.loadLibrary("ffmpeg");
	}

	public static native long ffmpeg_decode();

	public static native long ffmpeg_encode();

	public static native long ffmpeg_pcm_aac();

	public static native long ffmpeg_yuv_h264();

	public static native long ffmpeg_mp4_mkv();

	public static native long ffmpeg_merge();

	public static native long ffmpeg_remuxer();

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);

		findViewById(R.id.decode).setOnClickListener(this);
		findViewById(R.id.encode).setOnClickListener(this);
		findViewById(R.id.audio_pcm_aac).setOnClickListener(this);
		findViewById(R.id.video_yuv_h264).setOnClickListener(this);
		findViewById(R.id.video_mp4_mkv).setOnClickListener(this);
		findViewById(R.id.merge).setOnClickListener(this);
		findViewById(R.id.remuder).setOnClickListener(this);
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}

	@Override
	public void onClick(View v) {
		// TODO Auto-generated method stub
		switch (v.getId()) {
		case R.id.decode:
			ffmpeg_decode();
			Toast.makeText(this, "结束", 0).show();
			break;

		case R.id.encode:
			ffmpeg_encode();
			Toast.makeText(this, "结束", 0).show();
			break;

		case R.id.audio_pcm_aac:
			ffmpeg_pcm_aac();
			Toast.makeText(this, "结束", 0).show();
			break;

		case R.id.video_yuv_h264:
			ffmpeg_yuv_h264();
			Toast.makeText(this, "结束", 0).show();
			break;

		case R.id.video_mp4_mkv:
			Toast.makeText(this, "暂无实现", 0).show();
			// ffmpeg_mp4_mkv();
			break;

		case R.id.merge:
			ffmpeg_merge();
			break;

		case R.id.remuder:
			ffmpeg_remuxer();
			break;
		}
	}

}
