package org.opencmr2.vr;

import java.io.File;
import java.io.IOException;
import org.libsdl.app.SDLActivity;

/** ARM64 SDL entry point. OpenXR must run in the package's main process for Meta
 * compositor routing; the setup activity uses the separate :setup process. */
public final class VrActivity extends SDLActivity {
    private GameDataImporter.Lock dataLock;
    @Override protected String[] getLibraries() {
        return new String[] { "openxr_loader", "SDL3", "main" };
    }
    @Override protected String[] getArguments() {
        File root = getExternalFilesDir(null);
        if (root == null) root = getFilesDir();
        File data = GameDataImporter.dataDirectory(getFilesDir(), root);
        File user = new File(root, "User");
        user.mkdirs();
        if (getIntent().getBooleanExtra("vr_test", false))
            return new String[] { "--vr-test", "--user-dir", user.getAbsolutePath() };
        try { if (dataLock == null) dataLock = GameDataImporter.lock(getFilesDir()); }
        catch (IOException e) { throw new IllegalStateException("Game data is being imported. Please finish the import first.", e); }
        return new String[] { "--vr", "--data", data.getAbsolutePath(), "--user-dir", user.getAbsolutePath() };
    }
    @Override protected void onDestroy() {
        super.onDestroy();
        if (dataLock != null) try { dataLock.close(); } catch (IOException ignored) { }
    }
}
