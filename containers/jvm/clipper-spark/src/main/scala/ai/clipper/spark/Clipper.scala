package ai.clipper.spark

import java.net.URLClassLoader
import java.nio.file.StandardCopyOption.REPLACE_EXISTING
import java.nio.file.StandardOpenOption.CREATE
import java.nio.file.{Files, Paths}
import java.nio.charset.Charset

import org.apache.spark.SparkContext
import org.apache.spark.ml.PipelineModel
import org.json4s._
import org.json4s.jackson.Serialization.{read, write}
import org.json4s.jackson.Serialization

import scalaj.http._

import scala.sys.process._

sealed trait ModelType extends Serializable

case object MLlibModelType extends ModelType

case object PipelineModelType extends ModelType

object ModelTypeSerializer
    extends CustomSerializer[ModelType](
      _ =>
        (
          {
            case JString("MLlibModelType") => MLlibModelType
            case JString("PipelineModelType") => PipelineModelType
          }, {
            case MLlibModelType => JString("MLlibModelType")
            case PipelineModelType => JString("PipelineModelType")
          }
      ))

class UnsupportedEnvironmentException(e: String) extends RuntimeException(e)

class ModelDeploymentError(e: String) extends RuntimeException(e)

case class ClipperContainerConf(var className: String,
                                var jarName: String,
                                var modelType: ModelType,
                                var fromRepl: Boolean = false,
                                var replClassDir: Option[String] = None)

object Clipper {

  val CLIPPER_CONF_FILENAME: String = "clipper_conf.json"
  val CONTAINER_JAR_FILE: String = "container_source.jar"
  val MODEL_DIRECTORY: String = "model"
  val REPL_CLASS_DIR: String = "repl_classes"
  val CLIPPER_SPARK_CONTAINER_NAME = "clipper/spark-scala-container"

  val DOCKER_NW: String = "clipper_nw"
  val CLIPPER_MANAGEMENT_PORT: Int = 1338

  val CLIPPER_DOCKER_LABEL: String = "ai.clipper.container.label";


  // Imports the json serialization library as an implicit and adds our custom serializer
  // for the ModelType case classes
  implicit val json4sFormats = Serialization.formats(NoTypeHints) + ModelTypeSerializer


  /**
    *
    * @param sc Spark context
    * @param name The name to assign the model when deploying to Clipper
    * @param version The model version
    * @param model The trained Spark model. Note that this _must_ be an instance of either
    *              ai.clipper.spark.MLlibModel or org.apache.spark.ml.PipelineModel
    * @param containerClass The model container which specifies how to use the trained
    *                       model to make predictions. This can include any pre-processing
    *                       or post-processing code (including any featurization). This class
    *                       must either extend ai.clipper.spark.MLlibContainer or
    *                       ai.clipper.spark.PipelineModelContainer.
    * @param clipperHost The IP address or hostname of a running Clipper instance. This can be either localhost
    *                    or a remote machine that you have SSH access to. SSH access is required to copy the model and
    *                    launch a Docker container on the remote machine.
    * @param labels A list of labels to be associated with the model.
    * @param sshUserName If deploying to a remote machine, the username associated with the SSH credentials.
    * @param sshKeyPath If deploying to a remote machine, the path to an SSH key authorized to log in to the remote
    *                   machine.
    * @param dockerRequiresSudo True if the Docker daemon on the machine hosting Clipper requires sudo to access. If
    *                           set to true, the ssh user you specify must have passwordless sudo access.
    * @tparam M The type of the model. This _must_ be an instance of either
    *              ai.clipper.spark.MLlibModel or org.apache.spark.ml.PipelineModel
    */
  def deploySparkModel[M](sc: SparkContext,
                          name: String,
                          version: Int,
                          model: M,
                          containerClass: Class[_],
                          clipperHost: String,
                          labels: List[String],
                          sshUserName: Option[String] = None,
                          sshKeyPath: Option[String] = None,
                          dockerRequiresSudo: Boolean = true): Unit = {
    val basePath = Paths.get("/tmp", name, version.toString).toString
    // Use the same directory scheme of /tmp/<name>/<version> on host
    val hostDataPath = basePath
    val localHostNames = Set("local", "localhost", "127.0.0.1")
    val islocalHost = localHostNames.contains(clipperHost.toLowerCase())

    try {
      saveSparkModel[M](sc, name, version, model, containerClass, basePath)
      if (!islocalHost) {
        // Make sure that ssh credentials were supplied
        val (user, key) = try {
          val user = sshUserName.get
          val key = sshKeyPath.get
          (user, key)
        } catch {
          case _: NoSuchElementException => {
            val err =
              "SSH user name and keypath must be supplied to deploy model to remote Clipper instance"
            println(err)
            throw new ModelDeploymentError(err)
          }
        }
        copyModelDataToHost(clipperHost, basePath, hostDataPath, user, key)
        publishModelToClipper(clipperHost, name, version, labels, hostDataPath)
        startSparkContainerRemote(name,
                                  version,
                                  clipperHost,
                                  hostDataPath,
                                  user,
                                  key,
                                  dockerRequiresSudo)
      } else {
        publishModelToClipper(clipperHost, name, version, labels, hostDataPath)
        startSparkContainerLocal(name, version, basePath)
      }
    } catch {
      case e: Throwable => {
        println(s"Error saving model: ${e.printStackTrace}")
        return
      }
    }
  }

  private def startSparkContainerLocal(name: String,
                                       version: Int,
                                       modelDataPath: String): Unit = {
    println(s"MODEL_DATA_PATH: $modelDataPath")
    val startContainerCmd = Seq(
      "docker",
      "run",
      "-d",
      s"--network=$DOCKER_NW",
      "-v", s"$modelDataPath:/model:ro",
      "-e", s"CLIPPER_MODEL_NAME=$name",
      "-e", s"CLIPPER_MODEL_VERSION=$version",
      "-e", "CLIPPER_IP=query_frontend",
      "-e", "CLIPPER_INPUT_TYPE=doubles",
      "-l", s"$CLIPPER_DOCKER_LABEL",
      CLIPPER_SPARK_CONTAINER_NAME
    )
    if (startContainerCmd.! != 0) {
      throw new ModelDeploymentError("Error starting model container")
    }
  }

  private def startSparkContainerRemote(name: String,
                                        version: Int,
                                        clipperHost: String,
                                        modelDataPath: String,
                                        sshUserName: String,
                                        sshKeyPath: String,
                                        dockerRequiresSudo: Boolean): Unit = {

    val sudoCommand = if (dockerRequiresSudo) Seq("sudo") else Seq()
    val getDockerIPCommand = sudoCommand ++ Seq(
      "docker",
      "ps", "-aqf",
      "ancestor=clipper/query_frontend",
      "|", "xargs") ++ sudoCommand ++ Seq("docker",
      "inspect",
      "--format='{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'"
    )



    val sshCommand = Seq("ssh",
      "-o", "StrictHostKeyChecking=no",
      "-i", s"$sshKeyPath",
      s"$sshUserName@$clipperHost")

    val dockerIpCommand = sshCommand ++ getDockerIPCommand
    val dockerIp = dockerIpCommand.!!.stripLineEnd
    println(s"Docker IP: $dockerIp")

    val startContainerCmd = sudoCommand ++ Seq(
      "docker",
      "run",
      "-d",
      s"--network=$DOCKER_NW",
      "-v", s"$modelDataPath:/model:ro",
      "-e", s"CLIPPER_MODEL_NAME=$name",
      "-e", s"CLIPPER_MODEL_VERSION=$version",
      "-e", s"CLIPPER_IP=$dockerIp",
      "-e", "CLIPPER_INPUT_TYPE=doubles",
      "-l", s"$CLIPPER_DOCKER_LABEL",
      CLIPPER_SPARK_CONTAINER_NAME
    )
    val sshStartContainerCmd = sshCommand ++ startContainerCmd
    println(sshStartContainerCmd)
    if (sshStartContainerCmd.! != 0) {
      throw new ModelDeploymentError("Error starting model container")
    }
  }

  private def publishModelToClipper(host: String,
                                    name: String,
                                    version: Int,
                                    labels: List[String],
                                    hostModelDataPath: String): Unit = {
    val data = Map(
      "model_name" -> name,
      "model_version" -> version,
      "labels" -> labels,
      "input_type" -> "doubles",
      "container_name" -> CLIPPER_SPARK_CONTAINER_NAME,
      "model_data_path" -> hostModelDataPath
    )
    val jsonData = write(data)


    val response = Http(s"http://$host:$CLIPPER_MANAGEMENT_PORT/admin/add_model")
      .header("Content-type", "application/json")
      .postData(jsonData)
      .asString

    if (response.code == 200) {
      println("Successfully published model to Clipper")
    } else {
      throw new ModelDeploymentError(
        s"Error publishing model to Clipper. ${response.code}: ${response.body}")
    }
  }

  private def copyModelDataToHost(host: String,
                                  localPath: String,
                                  destPath: String,
                                  sshUserName: String,
                                  sshKeyPath: String): Unit = {

    val mkdirCommand = Seq("ssh",
                           "-o", "StrictHostKeyChecking=no",
                           "-i", s"$sshKeyPath",
                           s"$sshUserName@$host",
                           "mkdir", "-p", destPath)
    val copyCommand = Seq("rsync",
                          "-r",
                          "-e", s"ssh -i $sshKeyPath -o StrictHostKeyChecking=no",
                          s"$localPath/",
                          s"$sshUserName@$host:$destPath")
    if (!(mkdirCommand.! == 0 && copyCommand.! == 0)) {
        throw new ModelDeploymentError(
        "Error copying model data to Clipper host")
    }
  }

  private[clipper] def saveSparkModel[M](sc: SparkContext,
                                         name: String,
                                         version: Int,
                                         model: M,
                                         containerClass: Class[_],
                                         basePath: String): Unit = {
    // Check that Spark is not running in the REPL
    if (getReplOutputDir(sc).isDefined) {
      throw new UnsupportedEnvironmentException(
        "Clipper cannot deploy models from Spark Shell")
    }
    val modelPath = Paths.get(basePath, MODEL_DIRECTORY).toString
    val modelType = model match {
      case m: MLlibModel => {
        m.save(sc, modelPath)
        // Because I'm not sure how to do it in the type system, check that
        // the container is of the right type
        // NOTE: this test doesn't work from the REPL
        try {
          containerClass.newInstance.asInstanceOf[MLlibContainer]
        } catch {
          case e: ClassCastException => {
            throw new IllegalArgumentException(
              "Error: Container must be a subclass of MLlibContainer")
          }
        }
        MLlibModelType
      }
      case p: PipelineModel => {
        p.save(modelPath)
        // Because I'm not sure how to do it in the type system, check that
        // the container is of the right type
        try {
          containerClass.newInstance.asInstanceOf[PipelineModelContainer]
        } catch {
          case e: ClassCastException => {
            throw new IllegalArgumentException(
              "Error: Container must be a subclass of PipelineModelContainer")
          }
        }
        PipelineModelType
      }
      case _ =>
        throw new IllegalArgumentException(
          s"Illegal model type: ${model.getClass.getName}")
    }
    val jarPath = Paths.get(
      containerClass.getProtectionDomain.getCodeSource.getLocation.getPath)
    val copiedJarName = CONTAINER_JAR_FILE
    println(s"JAR path: $jarPath")
    System.out.flush()
    Files.copy(jarPath, Paths.get(basePath, copiedJarName), REPLACE_EXISTING)
    val conf =
      ClipperContainerConf(containerClass.getName, copiedJarName, modelType)
    getReplOutputDir(sc) match {
      case Some(classSourceDir) => {
        throw new UnsupportedOperationException("Clipper does not support deploying models directly from the Spark REPL")
        // NOTE: This commented out code is intentionally committed. We hope to support
        // model deployment in the future.
//        println(
//          "deployModel called from Spark REPL. Saving classes defined in REPL.")
//        conf.fromRepl = true
//        conf.replClassDir = Some(REPL_CLASS_DIR)
//        val classDestDir = Paths.get(basePath, REPL_CLASS_DIR)
//        FileUtils.copyDirectory(Paths.get(classSourceDir).toFile,
//                                classDestDir.toFile)
      }
      case None =>
        println(
          "deployModel called from script. No need to save additionally generated classes.")
    }
    Files.write(Paths.get(basePath, CLIPPER_CONF_FILENAME),
                write(conf).getBytes,
                CREATE)
  }

  private def getReplOutputDir(sc: SparkContext): Option[String] = {
    sc.getConf.getOption("spark.repl.class.outputDir")
  }

  private[clipper] def loadSparkModel(
      sc: SparkContext,
      basePath: String): SparkModelContainer = {
    val confString = Files
      .readAllLines(Paths.get(basePath, CLIPPER_CONF_FILENAME),
                    Charset.defaultCharset())
      .get(0)

    val conf = read[ClipperContainerConf](confString)
    val classLoader = getClassLoader(basePath, conf)
    val modelPath = Paths.get(basePath, MODEL_DIRECTORY).toString
    println(s"Model path: $modelPath")

    conf.modelType match {
      case MLlibModelType => {
        val model = MLlibLoader.load(sc, modelPath)
        try {
          val container = classLoader
            .loadClass(conf.className)
            .newInstance()
            .asInstanceOf[MLlibContainer]
          container.init(sc, model)
          container.asInstanceOf[SparkModelContainer]
        } catch {
          case e: Throwable => {
            e.printStackTrace
            throw e
          }

        }
      }
      case PipelineModelType => {
        val model = PipelineModel.load(modelPath)
        try {
        val container = classLoader
          .loadClass(conf.className)
          .newInstance()
          .asInstanceOf[PipelineModelContainer]
        container.init(sc, model)
        container.asInstanceOf[SparkModelContainer]
        } catch {
          case e: Throwable => {
            e.printStackTrace
            throw e
          }
        }
      }
    }
  }

  private def getClassLoader(path: String,
                             conf: ClipperContainerConf): ClassLoader = {
    new URLClassLoader(Array(Paths.get(path, conf.jarName).toUri.toURL),
                       getClass.getClassLoader)
  }
}
