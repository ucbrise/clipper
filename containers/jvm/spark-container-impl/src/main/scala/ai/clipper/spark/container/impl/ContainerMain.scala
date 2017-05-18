package ai.clipper.spark.container.impl

import java.net.UnknownHostException

import ai.clipper.container.data.DoubleVector
import ai.clipper.rpc.RPC
import ai.clipper.spark.{Clipper, SparkModelContainer}
import org.apache.spark.{SparkConf, SparkContext}

object ContainerMain {

  def main(args: Array[String]): Unit = {

    val modelPath = sys.env("CLIPPER_MODEL_PATH")
    val modelName = sys.env("CLIPPER_MODEL_NAME")
    val modelVersion = sys.env("CLIPPER_MODEL_VERSION").toInt

    val clipperAddress = sys.env.getOrElse("CLIPPER_IP", "127.0.0.1")
    val clipperPort = sys.env.getOrElse("CLIPPER_PORT", "7000").toInt

    val conf = new SparkConf()
      .setAppName("ClipperSparkContainer")
      .setMaster("local")
    val sc = new SparkContext(conf)
    // Reduce logging noise
    sc.parallelize(Seq(""))
      .foreachPartition(x => {
        import org.apache.commons.logging.LogFactory
        import org.apache.log4j.{Level, LogManager}
        LogManager.getRootLogger().setLevel(Level.WARN)
        val log = LogFactory.getLog("EXECUTOR-LOG:")
        log.warn("START EXECUTOR WARN LOG LEVEL")
      })
    val container: SparkModelContainer = Clipper.loadSparkModel(sc, modelPath)
    val parser = new DoubleVector.Parser

    while (true) {
      println("Starting Clipper Spark Container")
      println(s"Serving model $modelName@$modelVersion")
      println(s"Connecting to Clipper at $clipperAddress:$clipperPort")

      val rpcClient = new RPC(parser)
      try {
        rpcClient.start(container, modelName, modelVersion, clipperAddress, clipperPort)
      } catch {
        case e: UnknownHostException => e.printStackTrace()
      }
    }
  }
}
