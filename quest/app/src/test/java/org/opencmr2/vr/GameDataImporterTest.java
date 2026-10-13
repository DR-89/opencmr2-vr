package org.opencmr2.vr;

import org.junit.Test;
import org.junit.Rule;
import org.junit.rules.TemporaryFolder;
import java.io.*;
import java.nio.file.Files;
import java.util.*;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.zip.*;
import static org.junit.Assert.*;

public class GameDataImporterTest {
    @Rule public TemporaryFolder temp = new TemporaryFolder();
    private static final GameDataImporter.Progress QUIET=(p,d,t,n)->{};
    private static final long PLENTY=20L<<30;
    private Map<String, byte[]> installation(String prefix, String marker) {
        Map<String,byte[]> entries=new LinkedHashMap<>();
        for(String path:GameDataImporter.REQUIRED) entries.put(prefix+path,(marker+" "+path).getBytes(java.nio.charset.StandardCharsets.UTF_8));
        entries.put(prefix+"Game/Tracks/Finland/1/track.dat",new byte[180000]);
        return entries;
    }
    private byte[] zip(Map<String,byte[]> entries) throws IOException {
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();
        try(ZipOutputStream out=new ZipOutputStream(bytes)) {
            for(Map.Entry<String,byte[]> item:entries.entrySet()) {
                ZipEntry entry=new ZipEntry(item.getKey());
                CRC32 crc=new CRC32(); crc.update(item.getValue());
                entry.setMethod(ZipEntry.STORED); entry.setSize(item.getValue().length); entry.setCrc(crc.getValue());
                out.putNextEntry(entry); out.write(item.getValue()); out.closeEntry();
            }
        }
        return bytes.toByteArray();
    }
    private GameDataImporter.Result run(File root,byte[] bytes) throws IOException {
        return GameDataImporter.run(new ByteArrayInputStream(bytes),bytes.length,root,QUIET,()->false,()->PLENTY);
    }
    private String installedText(File root) throws IOException {
        return new String(Files.readAllBytes(new File(root,"Data/game/bigfiles/common.bfl").toPath()),java.nio.charset.StandardCharsets.UTF_8);
    }
    private File existing() throws IOException {
        File root=temp.newFolder(); run(root,zip(installation("","old"))); return root;
    }
    private void rejected(File root,byte[] bytes,String message) throws IOException {
        IOException error=assertThrows(IOException.class,()->run(root,bytes));
        assertTrue(error.getMessage(),error.getMessage().contains(message));
        assertTrue(installedText(root).startsWith("old"));
        assertFalse(new File(root,".import-work").exists());
    }
    @Test public void rootAndNestedFoldersImportWithCaseInsensitiveNames() throws Exception {
        for(String prefix:new String[]{"","Colin McRae Rally 2/","Backup/Spiel/"}) {
            File root=temp.newFolder();
            Map<String,byte[]> entries=new LinkedHashMap<>();
            installation(prefix,"new").forEach((k,v)->entries.put(k.toUpperCase(Locale.ROOT),v));
            GameDataImporter.Result result=run(root,zip(entries));
            assertEquals(4,result.files); assertTrue(result.bytes>180000);
            assertTrue(GameDataImporter.installed(new File(root,"Data")));
            assertTrue(installedText(root).startsWith("new"));
            assertFalse(new File(root,".import-work").exists());
        }
    }
    @Test public void replacementKeepsSavesAndRemovesOldBackup() throws Exception {
        File root=existing();
        File user=new File(root,"User"); assertTrue(user.mkdir());
        Files.write(new File(user,"save.dat").toPath(),new byte[]{7});
        run(root,zip(installation("My game/","new")));
        assertTrue(installedText(root).startsWith("new"));
        assertArrayEquals(new byte[]{7},Files.readAllBytes(new File(user,"save.dat").toPath()));
        assertFalse(new File(root,".data-backup").exists());
    }
    @Test public void traversalAndAbsolutePathsCannotOverwriteOutsideStaging() throws Exception {
        File root=existing();
        for(String bad:new String[]{"../escape", "/escape", "C:/escape", "wrapper/../../Data/save", "a\\..\\escape", "a//b"}) {
            Map<String,byte[]> entries=installation("","new"); entries.put(bad,new byte[]{1});
            rejected(root,zip(entries),"file path");
        }
        assertFalse(new File(root.getParentFile(),"escape").exists());
    }
    @Test public void missingAndEmptyRequiredFilesGiveSpecificErrors() throws Exception {
        File root=existing(); Map<String,byte[]> entries=installation("Spiel/","new");
        entries.remove("Spiel/Sounds/surface.bfl");
        rejected(root,zip(entries),"Sounds/surface.bfl");
        entries=installation("","new"); entries.put("FrontEnd/common.bfl",new byte[0]);
        rejected(root,zip(entries),"FrontEnd/common.bfl");
    }
    @Test public void duplicateCaseAndFileDirectoryConflictsAreRejected() throws Exception {
        File root=existing(); Map<String,byte[]> entries=installation("","new");
        entries.put("GAME/BIGFILES/COMMON.BFL",new byte[]{9}); rejected(root,zip(entries),"Duplicate");
        entries=installation("","new"); entries.put("Game",new byte[]{9}); rejected(root,zip(entries),"folder structure");
    }
    @Test public void multipleInstallationsAreRejected() throws Exception {
        File root=existing(); Map<String,byte[]> entries=installation("one/","new");
        entries.putAll(installation("two/","other")); rejected(root,zip(entries),"multiple");
    }
    @Test public void badZipAndBadCrcLeaveOriginalData() throws Exception {
        File root=existing(); rejected(root,new byte[]{1,2,3,4},"ZIP");
        byte[] bytes=zip(installation("","BADCRC"));
        byte[] marker="BADCRC".getBytes(java.nio.charset.StandardCharsets.UTF_8);
        for(int i=0;i<bytes.length-marker.length;i++) {
            if(Arrays.equals(Arrays.copyOfRange(bytes,i,i+marker.length),marker)) { bytes[i]='X'; break; }
        }
        rejected(root,bytes,"Damaged ZIP");
    }
    @Test public void insufficientSpaceBeforeAndDuringExtractionPreservesData() throws Exception {
        File root=existing(); byte[] bytes=zip(installation("","new"));
        IOException before=assertThrows(IOException.class,()->GameDataImporter.run(new ByteArrayInputStream(bytes),bytes.length,root,QUIET,()->false,()->1));
        assertTrue(before.getMessage().contains("storage space"));
        AtomicBoolean extracting=new AtomicBoolean();
        IOException during=assertThrows(IOException.class,()->GameDataImporter.run(new ByteArrayInputStream(bytes),bytes.length,root,
                (p,d,t,n)->{if(p.equals("Importing game data"))extracting.set(true);},()->false,()->extracting.get()?1:PLENTY));
        assertTrue(during.getMessage().contains("storage space"));
        assertTrue(installedText(root).startsWith("old"));
    }
    @Test public void cancellationDuringCopyAndExtractionPreservesData() throws Exception {
        for(String phase:new String[]{"Reading ZIP","Importing game data"}) {
            File root=existing(); byte[] bytes=zip(installation("","new")); AtomicBoolean cancel=new AtomicBoolean();
            assertThrows(GameDataImporter.Canceled.class,()->GameDataImporter.run(new ByteArrayInputStream(bytes),bytes.length,root,
                    (p,d,t,n)->{if(p.equals(phase)&&d>0)cancel.set(true);},cancel::get,()->PLENTY));
            assertTrue(installedText(root).startsWith("old")); assertFalse(new File(root,".import-work").exists());
        }
    }
    @Test public void truncatedTransferAndOversizedZipAreRejected() throws Exception {
        File root=existing(); byte[] bytes=zip(installation("","new"));
        assertThrows(IOException.class,()->GameDataImporter.run(new ByteArrayInputStream(bytes),bytes.length+1,root,QUIET,()->false,()->PLENTY));
        assertThrows(IOException.class,()->GameDataImporter.run(new ByteArrayInputStream(bytes),GameDataImporter.MAX_ARCHIVE+1,root,QUIET,()->false,()->PLENTY));
        assertTrue(installedText(root).startsWith("old"));
    }
    @Test public void unknownSourceLengthIsSupported() throws Exception {
        File root=temp.newFolder(); byte[] bytes=zip(installation("","new"));
        GameDataImporter.run(new ByteArrayInputStream(bytes),-1,root,QUIET,()->false,()->PLENTY);
        assertTrue(GameDataImporter.installed(new File(root,"Data")));
    }
    @Test public void crashBeforeCommitRestoresBackupAndCleansStaging() throws Exception {
        File root=existing(); assertTrue(new File(root,"Data").renameTo(new File(root,".data-backup")));
        assertTrue(new File(root,".import-work/payload").mkdirs());
        GameDataImporter.recover(root);
        assertTrue(installedText(root).startsWith("old")); assertFalse(new File(root,".import-work").exists());
    }
    @Test public void crashAfterCommitKeepsNewData() throws Exception {
        File root=existing(); File other=temp.newFolder(); run(other,zip(installation("","new")));
        assertTrue(new File(root,"Data").renameTo(new File(root,".data-backup")));
        assertTrue(new File(other,"Data").renameTo(new File(root,"Data")));
        GameDataImporter.recover(root);
        assertTrue(installedText(root).startsWith("new")); assertFalse(new File(root,".data-backup").exists());
    }
    @Test public void runningGameLockPreventsImportOrCleanup() throws Exception {
        File root=existing();
        try(GameDataImporter.Lock ignored=GameDataImporter.lock(root)) {
            rejected(root,zip(installation("","new")),"still running");
            assertThrows(IOException.class,()->GameDataImporter.recover(root));
        }
        run(root,zip(installation("","new"))); assertTrue(installedText(root).startsWith("new"));
    }
}
