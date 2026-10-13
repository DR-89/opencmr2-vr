package org.opencmr2.vr;

import java.io.*;
import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;
import java.nio.channels.OverlappingFileLockException;
import java.nio.file.Files;
import java.util.*;
import java.util.function.BooleanSupplier;
import java.util.function.LongSupplier;
import java.util.zip.*;

/** Local ZIP import; deliberately independent of Android for filesystem regression tests. */
public final class GameDataImporter {
    static final long MAX_ARCHIVE = 4L << 30, MAX_DATA = 8L << 30, RESERVE = 64L << 20;
    static final int MAX_ENTRIES = 50000;
    static final String[] REQUIRED = {"Game/Bigfiles/common.bfl", "FrontEnd/common.bfl", "Sounds/surface.bfl"};
    public interface Progress { void update(String phase, long done, long total, String detail); }
    public static final class Result {
        public final int files;
        public final long bytes;
        Result(int files, long bytes) { this.files = files; this.bytes = bytes; }
    }
    public static final class Canceled extends IOException {
        Canceled() { super("Import canceled. Your existing game data has been kept."); }
    }
    public static final class Busy extends IOException {
        Busy() { super("The game or another import is still running. Close the game before importing."); }
    }
    /** Shared by the importer and the VR process: data cannot change during a race. */
    public static final class Lock implements AutoCloseable {
        private final FileChannel channel;
        private final FileLock lock;
        Lock(File root) throws IOException {
            mkdir(root);
            channel = new RandomAccessFile(new File(root, ".data-lock"), "rw").getChannel();
            FileLock acquired;
            try { acquired = channel.tryLock(); }
            catch (OverlappingFileLockException e) { acquired = null; }
            catch (IOException e) { channel.close(); throw e; }
            if (acquired == null) {
                channel.close();
                throw new Busy();
            }
            lock = acquired;
        }
        @Override public void close() throws IOException { try { lock.release(); } finally { channel.close(); } }
    }
    public static Lock lock(File root) throws IOException { return new Lock(root); }

    private static File casePath(File base, String path) {
        File current = base;
        for (String part : path.split("/")) {
            File[] children = current.listFiles();
            if (children == null) return new File(base, path);
            File found = null;
            for (File child : children) if (child.getName().equalsIgnoreCase(part)) { found = child; break; }
            if (found == null) return new File(base, path);
            current = found;
        }
        return current;
    }
    public static boolean installed(File data) {
        for (String path : REQUIRED) {
            File file = casePath(data, path);
            if (!file.isFile() || file.length() == 0) return false;
        }
        return true;
    }
    public static File dataDirectory(File internal, File external) {
        File data = new File(internal, "Data");
        if (installed(data) || external == null) return data;
        File legacy = new File(external, "Data");
        return installed(legacy) ? legacy : data;
    }
    public static void recover(File root) throws IOException {
        try (Lock ignored = lock(root)) { recoverLocked(root); }
    }
    private static void recoverLocked(File root) throws IOException {
        File data = new File(root, "Data"), backup = new File(root, ".data-backup");
        if (backup.exists()) {
            if (!installed(data)) {
                if (data.exists()) deleteTree(root, data);
                if (!backup.renameTo(data)) throw new IOException("Could not restore the previous game data. Please reopen setup.");
            } else deleteTree(root, backup);
        }
        deleteTree(root, new File(root, ".import-work"));
    }
    public static Result run(InputStream source, long sourceSize, File root, Progress progress,
                             BooleanSupplier canceled) throws IOException {
        return run(source, sourceSize, root, progress, canceled, root::getUsableSpace);
    }
    static Result run(InputStream source, long sourceSize, File root, Progress progress,
                      BooleanSupplier canceled, LongSupplier space) throws IOException {
        try (InputStream input = source; Lock ignored = lock(root)) {
            recoverLocked(root);
            File work = new File(root, ".import-work");
            mkdir(work);
            try {
                File archive = new File(work, "selected.zip");
                copyArchive(input, sourceSize, archive, progress, canceled, space);
                progress.update("Checking ZIP", 0, 0, "Checking folders and required game files …");
                checkCanceled(canceled);
                try (ZipFile zip = new ZipFile(archive)) {
                    Plan plan = inspect(zip, canceled);
                    requireSpace(space, plan.bytes);
                    progress.update("Importing game data", 0, plan.bytes,
                            plan.files.size() + " files · " + size(plan.bytes) + " required · " + size(space.getAsLong()) + " available");
                    File payload = new File(work, "payload");
                    mkdir(payload);
                    long done = 0;
                    byte[] buffer = new byte[65536];
                    for (Map.Entry<String, ZipEntry> item : plan.files.entrySet()) {
                        checkCanceled(canceled);
                        File output = new File(payload, item.getKey());
                        assertInside(payload, output);
                        mkdir(output.getParentFile());
                        CRC32 crc = new CRC32();
                        long written = 0;
                        try (InputStream entry = zip.getInputStream(item.getValue());
                             FileOutputStream out = new FileOutputStream(output)) {
                            int count;
                            while ((count = entry.read(buffer)) != -1) {
                                checkCanceled(canceled);
                                written += count;
                                if (written > item.getValue().getSize()) throw new IOException("Invalid file size in ZIP: " + item.getKey());
                                requireSpace(space, count);
                                out.write(buffer, 0, count); crc.update(buffer, 0, count); done += count;
                                progress.update("Importing game data", done, plan.bytes, item.getKey());
                            }
                        }
                        if (written != item.getValue().getSize() || crc.getValue() != item.getValue().getCrc())
                            throw new IOException("Damaged ZIP entry: " + item.getKey() + ". Please create or copy the ZIP again.");
                    }
                    if (!installed(payload)) throw new IOException("Required game files are missing after extraction.");
                    checkCanceled(canceled);
                    progress.update("Finishing import", plan.bytes, plan.bytes, "Installing the verified game data …");
                    commit(root, payload);
                    return new Result(plan.files.size(), plan.bytes);
                } catch (ZipException e) {
                    throw new IOException("This ZIP is damaged or unsupported. Create a standard, unencrypted ZIP of the installed PC game folder.", e);
                }
            } finally { deleteTree(root, work); }
        }
    }
    private static void copyArchive(InputStream input, long size, File archive, Progress progress,
                                    BooleanSupplier canceled, LongSupplier space) throws IOException {
        if (size > MAX_ARCHIVE) throw new IOException("The ZIP exceeds 4 GiB. Include only this PC game installation.");
        requireSpace(space, Math.max(0, size));
        byte[] buffer = new byte[65536]; long done = 0;
        try (FileOutputStream out = new FileOutputStream(archive)) {
            int count;
            while ((count = input.read(buffer)) != -1) {
                checkCanceled(canceled);
                done += count;
                if (done > MAX_ARCHIVE) throw new IOException("The ZIP exceeds 4 GiB.");
                requireSpace(space, count);
                out.write(buffer, 0, count);
                progress.update("Reading ZIP", done, size, GameDataImporter.size(done) + (size > 0 ? " / " + GameDataImporter.size(size) : " read"));
            }
        }
        if (size >= 0 && done != size) throw new IOException("The ZIP could not be read completely. Please select it again.");
    }
    private static final class Plan {
        final Map<String, ZipEntry> files = new LinkedHashMap<>();
        long bytes;
    }
    private static Plan inspect(ZipFile zip, BooleanSupplier canceled) throws IOException {
        Map<String, ZipEntry> entries = new LinkedHashMap<>();
        Set<String> directories = new HashSet<>();
        int count = 0;
        Enumeration<? extends ZipEntry> all = zip.entries();
        while (all.hasMoreElements()) {
            checkCanceled(canceled);
            if (++count > MAX_ENTRIES) throw new IOException("Too many entries in the ZIP. Include only the game folder.");
            ZipEntry entry = all.nextElement();
            String path = safePath(entry.getName());
            if (entry.isDirectory()) directories.add(path);
            else if (entries.put(path, entry) != null) throw new IOException("Duplicate file names in ZIP: " + path);
        }
        for (String file : entries.keySet()) {
            if (directories.contains(file)) throw new IOException("A file and folder have the same name: " + file);
            String parent = file;
            while (parent.contains("/")) {
                parent = parent.substring(0, parent.lastIndexOf('/'));
                if (entries.containsKey(parent)) throw new IOException("Invalid folder structure: " + parent);
            }
        }
        List<String> candidates = new ArrayList<>(), complete = new ArrayList<>();
        String first = REQUIRED[0].toLowerCase(Locale.ROOT);
        for (String path : entries.keySet()) if (path.equals(first) || path.endsWith("/" + first)) {
            String prefix = path.substring(0, path.length() - first.length());
            candidates.add(prefix);
            if (missing(entries, prefix).isEmpty()) complete.add(prefix);
        }
        if (complete.size() > 1) throw new IOException("The ZIP contains multiple game installations. Include just one game folder.");
        if (complete.isEmpty()) {
            List<String> absent = missing(entries, candidates.isEmpty() ? "" : candidates.get(0));
            throw new IOException("No complete PC game installation found. Missing or empty files:\n" + String.join("\n", absent)
                    + "\nZIP the entire installed game folder, including Game, FrontEnd, Sounds and the other game files.");
        }
        String prefix = complete.get(0);
        Plan plan = new Plan();
        for (Map.Entry<String, ZipEntry> item : entries.entrySet()) if (item.getKey().startsWith(prefix)) {
            long bytes = item.getValue().getSize();
            if (bytes < 0 || bytes > MAX_DATA - plan.bytes) throw new IOException("Extracted data exceeds 8 GiB or the ZIP is invalid.");
            plan.files.put(item.getKey().substring(prefix.length()), item.getValue());
            plan.bytes += bytes;
        }
        return plan;
    }
    private static List<String> missing(Map<String, ZipEntry> entries, String prefix) {
        List<String> result = new ArrayList<>();
        for (String required : REQUIRED) {
            ZipEntry entry = entries.get(prefix + required.toLowerCase(Locale.ROOT));
            if (entry == null || entry.getSize() <= 0) result.add(required);
        }
        return result;
    }
    private static String safePath(String name) throws IOException {
        String path = name.replace('\\', '/');
        if (path.endsWith("/")) path = path.substring(0, path.length() - 1);
        String[] parts = path.split("/", -1);
        if (path.length() > 1024 || parts.length > 32 || path.indexOf(':') >= 0 || path.indexOf('\0') >= 0)
            throw new IOException("Invalid file path in ZIP.");
        for (String part : parts) if (part.isEmpty() || part.equals(".") || part.equals(".."))
            throw new IOException("Unsafe file path in ZIP: " + name);
        return path.toLowerCase(Locale.ROOT);
    }
    private static void commit(File root, File payload) throws IOException {
        File data = new File(root, "Data"), backup = new File(root, ".data-backup");
        if (data.exists() && !data.renameTo(backup)) throw new IOException("Could not back up the existing game data.");
        if (!payload.renameTo(data)) {
            if (backup.exists() && !backup.renameTo(data)) throw new IOException("Import interrupted. Reopen setup to restore the previous game data.");
            throw new IOException("Could not install the new game data. Your previous data has been kept.");
        }
        // A crash here is recovered on the next setup launch: the validated Data wins.
        deleteTree(root, backup);
    }
    private static void requireSpace(LongSupplier space, long bytes) throws IOException {
        long free = space.getAsLong();
        if (free < bytes + RESERVE) throw new IOException("Not enough storage space: " + size(bytes + RESERVE)
                + " required, " + size(free) + " available. Free up space on your Quest and select the ZIP again.");
    }
    private static void checkCanceled(BooleanSupplier canceled) throws Canceled {
        if (canceled.getAsBoolean() || Thread.currentThread().isInterrupted()) throw new Canceled();
    }
    private static void mkdir(File dir) throws IOException {
        if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException("Could not create folder: " + dir.getName());
    }
    private static void assertInside(File root, File file) throws IOException {
        if (!file.getCanonicalPath().startsWith(root.getCanonicalPath() + File.separator))
            throw new IOException("The file path is outside the import folder.");
    }
    private static void deleteTree(File root, File file) throws IOException {
        if (!file.exists() && !Files.isSymbolicLink(file.toPath())) return;
        // Never follow symlinks during cleanup, even if a previous local tool made one.
        if (Files.isSymbolicLink(file.toPath())) { Files.delete(file.toPath()); return; }
        assertInside(root, file);
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children == null) throw new IOException("Could not read the import folder.");
            for (File child : children) deleteTree(root, child);
        }
        if (!file.delete()) throw new IOException("Could not remove temporary import file: " + file.getName());
    }
    public static String size(long bytes) { return String.format(Locale.US, "%.1f MiB", bytes / 1048576.0); }
}
