package ai.clipper.spark

import java.nio.file.{Files, Path, Paths}

import org.apache.spark.{SparkConf, SparkContext}
import org.apache.spark.ml.PipelineModel
import org.apache.spark.mllib.classification.{LogisticRegressionModel, LogisticRegressionWithLBFGS, NaiveBayesModel, SVMModel}
import org.apache.spark.mllib.clustering.{BisectingKMeansModel, GaussianMixtureModel, KMeansModel}
import org.apache.spark.mllib.linalg.{Vector, Vectors}
import org.apache.spark.mllib.recommendation.MatrixFactorizationModel
import org.apache.spark.mllib.regression.{IsotonicRegressionModel, LassoModel, LinearRegressionModel, RidgeRegressionModel}
import org.apache.spark.mllib.tree.model.{DecisionTreeModel, GradientBoostedTreesModel, RandomForestModel}
import org.scalatest.{BeforeAndAfter, FunSuite}

import sys.process._

class TestContainer extends MLlibContainer {

  var model: Option[MLlibModel] = None

  override def init(sc: SparkContext, m: MLlibModel) {
    println("Initializing container")
    model = Some(m)
  }

  override def predict(xs: List[Vector]): List[Float] = {
    println("making prediction")
    val m = model.get
    xs.map(x => m.predict(x).toFloat)
  }
}

class MLLibModelSuite extends FunSuite with BeforeAndAfter {

  var sc: SparkContext = _
  var saveDir: Path = _

  before {
    val conf = new SparkConf()
      .setAppName("MLLibModelSuite")
      .setMaster("local")
    sc = new SparkContext(conf)
    sc.parallelize(Seq("")).foreachPartition(x => {
      import org.apache.commons.logging.LogFactory
      import org.apache.log4j.{Level, LogManager}
      LogManager.getRootLogger().setLevel(Level.WARN)
      val log = LogFactory.getLog("EXECUTOR-LOG:")
//      log.warn("START EXECUTOR WARN LOG LEVEL")
    })
    saveDir = Files.createTempDirectory(Paths.get("/tmp"), "clipper_unit_tests")
  }

  after {
    sc.stop()
    s"rm -rf ${saveDir.toString}".!
  }

  test("save and load MLlibLogisticRegressionModel") {
    val threshold = Vectors.dense(Array[Double](1.1,2.2,3.3,4.4))
    val intercept = 17
    val model = MLlibLogisticRegressionModel(new LogisticRegressionModel(threshold, intercept))

    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, model)
    Clipper.saveSparkModel(sc, "logistic_regression_test", 1, model, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibLogisticRegressionModel]
    assert(afterSaveModel.model.intercept == afterSaveModel.model.intercept)
    assert(afterSaveModel.model.weights == afterSaveModel.model.weights)
  }
}