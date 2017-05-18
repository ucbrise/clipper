package ai.clipper.rpc;

public class PerformanceTimer {
  static long lastLog;
  static StringBuilder log;

  public synchronized static void startTiming() {
    log = new StringBuilder();
    lastLog = System.currentTimeMillis();
  }

  public synchronized static void logElapsed(String tag) {
    long currTime = System.currentTimeMillis();
    String logMessage = tag + String.format(": %d", currTime - lastLog);
    log.append(logMessage);
    log.append("\n");
    lastLog = currTime;
  }

  public static synchronized String getLog() {
    return log.toString();
  }
}
