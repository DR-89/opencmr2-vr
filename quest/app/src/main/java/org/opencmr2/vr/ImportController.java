package org.opencmr2.vr;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.OpenableColumns;
import android.util.Log;
import java.io.*;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Retained across Activity recreation; the worker only holds application context. */
final class ImportController {
    static final class State {
        final boolean busy, success, error;
        final String title, detail;
        final long done, total;
        State(boolean busy, boolean success, boolean error, String title, String detail, long done, long total) {
            this.busy=busy; this.success=success; this.error=error; this.title=title; this.detail=detail; this.done=done; this.total=total;
        }
    }
    final Context context;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private volatile boolean canceled;
    volatile boolean blocked;
    private volatile State state = new State(true,false,false,"Checking game data", "Checking your existing installation …",0,0);
    private Runnable observer;
    private long lastProgress;
    ImportController(Context context) {
        this.context=context.getApplicationContext();
        worker.execute(() -> {
            try {
                GameDataImporter.recover(this.context.getFilesDir());
                publish(new State(false,false,false,installed()?"Ready to play":"Add your game data",
                        installed()?"Your PC game data is already installed on this Quest.":"Select a ZIP of your own PC game installation.",0,0));
            } catch (IOException e) { failed(e); }
        });
    }
    boolean installed() { return GameDataImporter.installed(GameDataImporter.dataDirectory(context.getFilesDir(),context.getExternalFilesDir(null))); }
    State state() { return state; }
    void attach(Runnable observer) { this.observer=observer; observer.run(); }
    void detach() { observer=null; }
    private void publish(State next) { state=next; main.post(() -> { if(observer!=null) observer.run(); }); }
    void start(Uri uri) {
        if(state.busy) return;
        canceled=false; blocked=false; lastProgress=0;
        publish(new State(true,false,false,"Opening ZIP","Opening the selected file …",0,0));
        worker.execute(() -> {
            try {
                long size=-1;
                try (Cursor cursor=context.getContentResolver().query(uri,new String[]{OpenableColumns.SIZE},null,null,null)) {
                    if(cursor!=null && cursor.moveToFirst() && !cursor.isNull(0)) size=cursor.getLong(0);
                }
                InputStream input=context.getContentResolver().openInputStream(uri);
                if(input==null) throw new IOException("Could not open the ZIP. Please select it again.");
                GameDataImporter.Result result=GameDataImporter.run(input,size,context.getFilesDir(),(phase,done,total,detail) -> {
                    long now=SystemClock.uptimeMillis();
                    if(now-lastProgress>=100 || !state.title.equals(phase)) {
                        lastProgress=now;
                        publish(new State(true,false,false,phase,detail,done,total));
                    }
                },() -> canceled);
                publish(new State(false,true,false,"Import complete",result.files+" files · "+GameDataImporter.size(result.bytes)
                        +"\nYou can now play. You can delete the original ZIP from Downloads when you no longer need it.",result.bytes,result.bytes));
                Log.i("OpenCMR2-Import","Import complete: "+result.files+" files, "+result.bytes+" bytes");
            } catch (GameDataImporter.Canceled e) {
                publish(new State(false,false,false,"Import canceled",e.getMessage(),0,0));
            } catch (Exception e) { failed(e); }
        });
    }
    void cancel() { canceled=true; }
    void close() { canceled=true; detach(); worker.shutdown(); }
    private void failed(Exception e) {
        blocked=e instanceof GameDataImporter.Busy;
        Log.w("OpenCMR2-Import","Import/setup failed",e);
        String text=e.getMessage();
        if(text==null || !(e instanceof IOException)) text="Could not read this file. Select a local ZIP from Downloads and try again.";
        publish(new State(false,false,true,"Unable to import",text,0,0));
    }
}
