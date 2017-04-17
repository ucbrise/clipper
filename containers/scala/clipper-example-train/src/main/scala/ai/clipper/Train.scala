
package ai.clipper.example

// import scala.reflect.runtime.universe._
import scala.reflect.runtime.universe
import scala.reflect.runtime.universe.TypeTag
import scala.reflect.api
import java.io._
import scala.io.Source


import org.apache.spark.{SparkConf, SparkContext}
import org.apache.spark.mllib.util.MLUtils
import org.apache.spark.mllib.linalg.Vector
import org.apache.spark.mllib.classification.{LogisticRegressionModel, LogisticRegressionWithLBFGS}

import ai.clipper.Clipper
import ai.clipper.container.{MLlibModel, MLlibLogisticRegressionModel,Container}


class LogisticRegressionContainer extends Container {

  // var model: Option[MLlibLogisticRegressionModel] = None
  var model: Option[MLlibModel] = None

  override def init(sc: SparkContext, m: MLlibModel) {
    // model = Some(model.asInstanceOf[MLlibLogisticRegressionModel)
    println("Initializing container")
    model = Some(m)
  }

  override def predict(x: Vector): Double = {
    println("making prediction")
    val m = model.get
    m.predict(x)
    // xs.map(m.predict(_))
  }
}

object Train {

  def main(args: Array[String]): Unit = {
    val conf = new SparkConf().setAppName("ClipperTest").setMaster("local[2]")
    val sc = new SparkContext(conf)
    sc.parallelize(Seq("")).foreachPartition(x => {
        import org.apache.log4j.{LogManager, Level}
        import org.apache.commons.logging.LogFactory
        LogManager.getRootLogger().setLevel(Level.WARN)
        val log = LogFactory.getLog("EXECUTOR-LOG:")
        log.warn("START EXECUTOR WARN LOG LEVEL")
      })

    // Load and parse the data file.
    val data = MLUtils.loadLibSVMFile(
      sc,
      "/Users/crankshaw/model-serving/spark_serialization_project/spark_binary/data/mllib/sample_libsvm_data.txt")
    // Split the data into training and test sets (30% held out for testing)
    val splits = data.randomSplit(Array(0.7, 0.3))
    val (trainingData, testData) = (splits(0), splits(1))

    val numClasses = 2
    //  Empty categoricalFeaturesInfo indicates all features are continuous.
    // val categoricalFeaturesInfo = Map[Int, Int]()
    // val impurity = "gini"
    // val maxDepth = 5
    // val maxBins = 32

    val model = MLlibLogisticRegressionModel(new LogisticRegressionWithLBFGS().setNumClasses(numClasses).run(trainingData))

    // val model = MLlibLogisticRegressionModel(LogisticRegressionModelWithSGD.train(trainingData,
    //                                          numClasses,
    //                                          categoricalFeaturesInfo,
    //                                          impurity,
    //                                          maxDepth,
    //                                          maxBins))



    // Evaluate model on test instances and compute test error
    val labelAndPreds = testData.map { point =>
      val prediction = model.predict(point.features)
      (point.label, prediction)
    }
    val numWrong = labelAndPreds.filter(r => r._1 != r._2).count()
    println(s"Num wrong: $numWrong")
    println("Learned logistic regression model:\n" + model.toString)

    // Clipper.deployModel(sc, "test", 1, model, "ai.clipper.example.LogisticRegressionContainer")
    Clipper.deployModel(sc, "test", 1, model, (new LogisticRegressionContainer).getClass.getName)
    sc.stop()
  }
}


// GaussianMixtureModel
// KMeansModel
// LDAModel
// PowerIterationClusteringModel
// ChiSqSelectorModel
// Word2VecModel
// FPGrowthModel
// PrefixSpanModel
// MatrixFactorizationModel
// IsotonicRegressionModel
// LassoModel
// LinearRegressionModel
// RidgeRegressionModel
// DecisionTreeModel
// RandomForestModel
// GradientBoostedTreesModel
