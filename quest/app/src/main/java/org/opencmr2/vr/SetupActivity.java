package org.opencmr2.vr;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import android.widget.*;

/** A Horizon 2D panel, shown before creating SDL/OpenXR or requesting a game file. */
public final class SetupActivity extends Activity {
    private static final int PICK_ZIP=41;
    private ImportController controller;
    private TextView status, detail, percent;
    private ProgressBar progress;
    private Button choose, play, diagnostic, cancel;
    private boolean autoStart, launched;
    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        setContentView(R.layout.setup);
        status=findViewById(R.id.status_title); detail=findViewById(R.id.status_detail);
        percent=findViewById(R.id.progress_label); progress=findViewById(R.id.progress);
        choose=findViewById(R.id.choose_zip); play=findViewById(R.id.play);
        diagnostic=findViewById(R.id.diagnostic); cancel=findViewById(R.id.cancel);
        autoStart=saved==null && !getIntent().getBooleanExtra("manage_data",false)
                && !getComponentName().getClassName().endsWith("DataSetup");
        controller=(ImportController)getLastNonConfigurationInstance();
        if(controller==null) controller=new ImportController(this);
        choose.setOnClickListener(v -> {
            autoStart=false;
            if(controller.installed()) new AlertDialog.Builder(this)
                    .setTitle(R.string.replace_title)
                    .setMessage(R.string.replace_message)
                    .setNegativeButton(R.string.back,null).setPositiveButton(R.string.select_zip,(d,w) -> pickZip()).show();
            else pickZip();
        });
        play.setOnClickListener(v -> launchGame(false));
        diagnostic.setOnClickListener(v -> launchGame(true));
        cancel.setOnClickListener(v -> { controller.cancel(); cancel.setEnabled(false); cancel.setText(R.string.canceling); });
        controller.attach(this::render);
    }
    private void render() {
        if(isFinishing() || launched) return;
        ImportController.State s=controller.state();
        status.setText(s.title); detail.setText(s.detail);
        status.setTextColor(Color.parseColor(s.error?"#FFB5A7":s.success?"#88E3B2":"#F2F5FA"));
        progress.setVisibility(s.busy?View.VISIBLE:View.GONE);
        percent.setVisibility(s.busy?View.VISIBLE:View.GONE);
        progress.setIndeterminate(s.total<=0);
        int value=s.total>0?(int)Math.min(100,s.done*100/s.total):0;
        progress.setProgress(value); percent.setText(s.total>0?getString(R.string.progress_percent,value):getString(R.string.please_wait));
        choose.setEnabled(!s.busy); diagnostic.setEnabled(!s.busy);
        play.setVisibility(!s.busy && !controller.blocked && controller.installed()?View.VISIBLE:View.GONE);
        cancel.setVisibility(s.busy && !s.title.equals("Checking game data") && !s.title.equals("Finishing import")?View.VISIBLE:View.GONE);
        if(!s.busy) { cancel.setEnabled(true); cancel.setText(R.string.cancel_import); }
        choose.setText(controller.installed()?R.string.replace_data:R.string.choose_zip);
        if(s.busy) getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        else getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if(!s.busy && autoStart) {
            autoStart=false;
            if(!s.error && controller.installed()) launchGame(false);
        }
    }
    private void pickZip() {
        Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE)
                .setType("*/*").putExtra(Intent.EXTRA_MIME_TYPES,new String[]{"application/zip","application/x-zip-compressed","application/octet-stream"})
                .putExtra(Intent.EXTRA_LOCAL_ONLY,true).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try { startActivityForResult(intent,PICK_ZIP); }
        catch(ActivityNotFoundException e) {
            new AlertDialog.Builder(this).setTitle(R.string.picker_unavailable)
                    .setMessage(R.string.picker_unavailable_message)
                    .setPositiveButton(R.string.ok,null).show();
        }
    }
    @Override protected void onActivityResult(int request,int result,Intent data) {
        super.onActivityResult(request,result,data);
        if(request==PICK_ZIP && result==RESULT_OK && data!=null && data.getData()!=null) {
            autoStart=false; controller.start(data.getData());
        }
    }
    private void launchGame(boolean test) {
        if(controller.state().busy || launched) return;
        launched=true;
        Intent intent=new Intent(this,VrActivity.class).setAction(Intent.ACTION_MAIN)
                .addCategory("com.oculus.intent.category.VR")
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK).putExtra("vr_test",test);
        startActivity(intent);
        finishAndRemoveTask();
    }
    @Override public Object onRetainNonConfigurationInstance() { return controller; }
    @Override public void onBackPressed() {
        if(controller.state().busy) {
            new AlertDialog.Builder(this).setTitle(R.string.cancel_title)
                    .setMessage(R.string.cancel_message)
                    .setNegativeButton(R.string.keep_importing,null)
                    .setPositiveButton(R.string.cancel,(d,w) -> controller.cancel()).show();
        } else super.onBackPressed();
    }
    @Override protected void onDestroy() {
        if(controller!=null) { controller.detach(); if(!isChangingConfigurations()) controller.close(); }
        super.onDestroy();
    }
}
