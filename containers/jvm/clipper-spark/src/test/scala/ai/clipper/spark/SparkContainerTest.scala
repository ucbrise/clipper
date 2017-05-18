package ai.clipper.spark

import java.nio.file.{Files, Path, Paths}

import com.holdenkarau.spark.testing.SharedSparkContext
import org.apache.spark
import org.apache.spark.{SparkConf, SparkContext}
import org.apache.spark.ml.PipelineModel
import org.apache.spark.mllib.classification._
import org.apache.spark.mllib.clustering._
import org.apache.spark.mllib.linalg.{Vector, Vectors}
import org.apache.spark.mllib.recommendation.{ALS, MatrixFactorizationModel, Rating}
import org.apache.spark.mllib.regression._
import org.apache.spark.mllib.tree.configuration.BoostingStrategy
import org.apache.spark.mllib.tree.{DecisionTree, GradientBoostedTrees, RandomForest}
import org.apache.spark.mllib.tree.model.{DecisionTreeModel, GradientBoostedTreesModel, RandomForestModel}
import org.apache.spark.mllib.util.MLUtils
import org.scalatest.{BeforeAndAfter, FunSuite, Matchers}

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

class MLLibModelSuite extends FunSuite with BeforeAndAfter with Matchers with SharedSparkContext {

  var saveDir: Path = _

  before {
    saveDir = Files.createTempDirectory(Paths.get("/tmp"), "clipper_unit_tests")
  }

  after {
    s"rm -rf ${saveDir.toString}".!
  }

  test("save and load MLlibLogisticRegressionModel") {

    val data = MLUtils.loadLibSVMFile(sc, getClass.getResource("/test-data/sample_libsvm_data.txt").toURI.toString)
    val beforeSaveModel = MLlibLogisticRegressionModel(new LogisticRegressionWithLBFGS()
      .setNumClasses(10)
      .run(data))
    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "logistic_regression_test", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibLogisticRegressionModel]
    beforeSaveModel.model.intercept shouldEqual afterSaveModel.model.intercept
    beforeSaveModel.model.getThreshold.get shouldEqual afterSaveModel.model.getThreshold.get
    beforeSaveModel.model.weights shouldEqual afterSaveModel.model.weights

  }

  test("save and load MLlibSVMModel") {
    val data = MLUtils.loadLibSVMFile(sc, getClass.getResource("/test-data/sample_libsvm_data.txt").toURI.toString)
    val beforeSaveModel = MLlibSVMModel(SVMWithSGD.train(data, 50))
    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "svm_test", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibSVMModel]
    beforeSaveModel.model.intercept shouldEqual afterSaveModel.model.intercept
    beforeSaveModel.model.weights shouldEqual afterSaveModel.model.weights

  }

  test("save and load MLlibNaiveBayesModel") {
    val data = MLUtils.loadLibSVMFile(sc, getClass.getResource("/test-data/sample_libsvm_data.txt").toURI.toString)
    val beforeSaveModel = MLlibNaiveBayesModel(NaiveBayes.train(data, lambda = 1.0, modelType = "multinomial"))

    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "naive_bayes_test", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibNaiveBayesModel]
    beforeSaveModel.model.labels shouldEqual afterSaveModel.model.labels
    beforeSaveModel.model.pi shouldEqual afterSaveModel.model.pi
    beforeSaveModel.model.theta shouldEqual afterSaveModel.model.theta
  }

  test("save and load MLlibBisectingKMeansModel") {

    val data = MLUtils.loadLibSVMFile(sc, getClass.getResource("/test-data/sample_kmeans_data.txt").toURI.toString).map(p => p.features)
    // Clustering the data into 6 clusters by BisectingKMeans.
    val bkm = new BisectingKMeans().setK(6)

    val beforeSaveModel = MLlibBisectingKMeansModel(bkm.run(data))

    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "bisecting_kmeans_test", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibBisectingKMeansModel]
    beforeSaveModel.model.clusterCenters shouldEqual afterSaveModel.model.clusterCenters
    beforeSaveModel.model.k shouldEqual afterSaveModel.model.k

  }

  test("save and load MLlibGaussianMixtureModel") {
    val data = MLUtils.loadLibSVMFile(sc, getClass.getResource("/test-data/sample_kmeans_data.txt").toURI.toString).map(p => p.features)

    val beforeSaveModel = MLlibGaussianMixtureModel(new GaussianMixture().setK(2).run(data))

    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "gaussian_mixture_model", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibGaussianMixtureModel]
    beforeSaveModel.model.weights shouldEqual afterSaveModel.model.weights
    for (i <- 0 until beforeSaveModel.model.gaussians.length) {
      beforeSaveModel.model.gaussians(i).mu shouldEqual afterSaveModel.model.gaussians(i).mu
      beforeSaveModel.model.gaussians(i).sigma shouldEqual afterSaveModel.model.gaussians(i).sigma
    }
  }

  test("save and load MLlibKMeansModel") {
    val data = MLUtils.loadLibSVMFile(sc, getClass.getResource("/test-data/sample_kmeans_data.txt").toURI.toString).map(p => p.features)

    val numClusters = 2
    val numIterations = 5

    val beforeSaveModel = MLlibKMeansModel(KMeans.train(data, numClusters, numIterations))

    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "kmeans_model", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibKMeansModel]
    beforeSaveModel.model.clusterCenters shouldEqual afterSaveModel.model.clusterCenters
    beforeSaveModel.model.k shouldEqual afterSaveModel.model.k
  }

  test("save and load MLlibMatrixFactorizationModel") {
    val data = sc.textFile(getClass.getResource("/test-data/als/test.data").toURI.toString)
    val ratings = data.map(_.split(',') match { case Array(user, item, rate) =>
      Rating(user.toInt, item.toInt, rate.toDouble)
    })
    // Build the recommendation model using ALS
    val rank = 10
    val numIterations = 10
    val beforeSaveModel = MLlibMatrixFactorizationModel(ALS.train(ratings, rank, numIterations, 0.01))
    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "matrix_factorization_model", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibMatrixFactorizationModel]

    val beforeUserFeatures: Array[(Int,Array[Double])] = beforeSaveModel.model.userFeatures.collect.sortBy(_._1)
    val afterUserFeatures: Array[(Int,Array[Double])] = afterSaveModel.model.userFeatures.collect.sortBy(_._1)

    for (i <- 0 until beforeUserFeatures.length) {
      beforeUserFeatures(i)._1 shouldEqual afterUserFeatures(i)._1
      beforeUserFeatures(i)._2 shouldEqual afterUserFeatures(i)._2
    }

    val beforeProductFeatures: Array[(Int,Array[Double])] = beforeSaveModel.model.productFeatures.collect.sortBy(_._1)
    val afterProductFeatures: Array[(Int,Array[Double])] = afterSaveModel.model.productFeatures.collect.sortBy(_._1)

    for (i <- 0 until beforeProductFeatures.length) {
      beforeProductFeatures(i)._1 shouldEqual afterProductFeatures(i)._1
      beforeProductFeatures(i)._2 shouldEqual afterProductFeatures(i)._2
    }
  }



  test("save and load MLlibIsotonicRegressionModel") {
    val data = MLUtils.loadLibSVMFile(sc,
      getClass.getResource("/test-data/sample_isotonic_regression_libsvm_data.txt").toURI.toString).cache()

    val parsedData = data.map { labeledPoint =>
      (labeledPoint.label, labeledPoint.features(0), 1.0)
    }
    val beforeSaveModel = MLlibIsotonicRegressionModel(new IsotonicRegression().setIsotonic(true).run(parsedData))
    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "isotonic_regression_model", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibIsotonicRegressionModel]
    beforeSaveModel.model.boundaries shouldEqual afterSaveModel.model.boundaries
    beforeSaveModel.model.predictions shouldEqual afterSaveModel.model.predictions
  }

  test("save and load MLlibLassoModel") {
    val data = sc.textFile(
      getClass.getResource("/test-data/ridge-data/lpsa.data").toURI.toString).cache()
    val parsedData = data.map { line =>
      val parts = line.split(',')
      LabeledPoint(parts(0).toDouble, Vectors.dense(parts(1).split(' ').map(_.toDouble)))
    }.cache()

    val numIterations = 100
    val beforeSaveModel = MLlibLassoModel(LassoWithSGD.train(parsedData, numIterations))
    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "lasso_regression_model", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibLassoModel]
    beforeSaveModel.model.weights shouldEqual afterSaveModel.model.weights
    beforeSaveModel.model.intercept shouldEqual afterSaveModel.model.intercept
  }

  test("save and load MLlibLinearRegressionModel") {
    val data = sc.textFile(
      getClass.getResource("/test-data/ridge-data/lpsa.data").toURI.toString).cache()
    val parsedData = data.map { line =>
      val parts = line.split(',')
      LabeledPoint(parts(0).toDouble, Vectors.dense(parts(1).split(' ').map(_.toDouble)))
    }.cache()

    val numIterations = 100
    val beforeSaveModel = MLlibLinearRegressionModel(LinearRegressionWithSGD.train(parsedData, numIterations))
    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "linear_regression_model", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibLinearRegressionModel]
    beforeSaveModel.model.weights shouldEqual afterSaveModel.model.weights
    beforeSaveModel.model.intercept shouldEqual afterSaveModel.model.intercept
  }

  test("save and load MLlibRidgeRegressionModel") {
    val data = sc.textFile(
      getClass.getResource("/test-data/ridge-data/lpsa.data").toURI.toString).cache()
    val parsedData = data.map { line =>
      val parts = line.split(',')
      LabeledPoint(parts(0).toDouble, Vectors.dense(parts(1).split(' ').map(_.toDouble)))
    }.cache()

    val numIterations = 100
    val beforeSaveModel = MLlibRidgeRegressionModel(RidgeRegressionWithSGD.train(parsedData, numIterations))
    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "ridge_regression_model", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibRidgeRegressionModel]
    beforeSaveModel.model.weights shouldEqual afterSaveModel.model.weights
    beforeSaveModel.model.intercept shouldEqual afterSaveModel.model.intercept
  }

  test("save and load MLlibDecisionTreeModel") {
    val data = MLUtils.loadLibSVMFile(sc, getClass.getResource("/test-data/sample_libsvm_data.txt").toURI.toString)
    // Train a DecisionTree model.
    //  Empty categoricalFeaturesInfo indicates all features are continuous.
    val numClasses = 2
    val categoricalFeaturesInfo = Map[Int, Int]()
    val impurity = "gini"
    val maxDepth = 5
    val maxBins = 32
    val model: DecisionTreeModel = DecisionTree.trainClassifier(data, numClasses, categoricalFeaturesInfo,
      impurity, maxDepth, maxBins)
    val beforeSaveModel = MLlibDecisionTreeModel(model)
    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "decision_tree_model", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibDecisionTreeModel]
    beforeSaveModel.model.topNode.impurity shouldEqual afterSaveModel.model.topNode.impurity
    beforeSaveModel.model.topNode.id shouldEqual afterSaveModel.model.topNode.id
    beforeSaveModel.model.topNode.stats.get.gain shouldEqual afterSaveModel.model.topNode.stats.get.gain
    beforeSaveModel.model.numNodes shouldEqual afterSaveModel.model.numNodes
  }

  test("save and load MLlibRandomForestModel") {
    val data = MLUtils.loadLibSVMFile(sc, getClass.getResource("/test-data/sample_libsvm_data.txt").toURI.toString)
    //  Empty categoricalFeaturesInfo indicates all features are continuous.
    val numClasses = 2
    val categoricalFeaturesInfo = Map[Int, Int]()
    val numTrees = 4 // Use more in practice.
    val featureSubsetStrategy = "auto" // Let the algorithm choose.
    val impurity = "gini"
    val maxDepth = 4
    val maxBins = 32
    val beforeSaveModel = MLlibRandomForestModel(RandomForest.trainClassifier(data, numClasses, categoricalFeaturesInfo,
      numTrees, featureSubsetStrategy, impurity, maxDepth, maxBins))
    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "random_forest_model", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibRandomForestModel]
    for (i <- 0 until beforeSaveModel.model.trees.length) {
      beforeSaveModel.model.trees(i).topNode.impurity shouldEqual afterSaveModel.model.trees(i).topNode.impurity
      beforeSaveModel.model.trees(i).topNode.id shouldEqual afterSaveModel.model.trees(i).topNode.id
      beforeSaveModel.model.trees(i).topNode.stats.get.gain shouldEqual afterSaveModel.model.trees(i).topNode.stats.get.gain
    }
  }

  test("save and load MLlibGradientBoostedTreesModel") {
    val data = MLUtils.loadLibSVMFile(sc, getClass.getResource("/test-data/sample_libsvm_data.txt").toURI.toString)
    val boostingStrategy = BoostingStrategy.defaultParams("Classification")
    boostingStrategy.setNumIterations(3) // Note: Use more iterations in practice.
    boostingStrategy.treeStrategy.setNumClasses(2)
    boostingStrategy.treeStrategy.setMaxDepth(5)
    // Empty categoricalFeaturesInfo indicates all features are continuous.
    val beforeSaveModel = MLlibGradientBoostedTreesModel(GradientBoostedTrees.train(data, boostingStrategy))
    val beforeSaveContainer = new TestContainer
    beforeSaveContainer.init(sc, beforeSaveModel)
    Clipper.saveSparkModel(sc, "gradient_boosted_trees_model", 1, beforeSaveModel, classOf[TestContainer], saveDir.toString)
    val afterSaveContainer = Clipper.loadSparkModel(sc, saveDir.toString)
    val afterSaveModel = afterSaveContainer.asInstanceOf[TestContainer].model.get.asInstanceOf[MLlibGradientBoostedTreesModel]
    for (i <- 0 until beforeSaveModel.model.trees.length) {
      beforeSaveModel.model.trees(i).topNode.impurity shouldEqual afterSaveModel.model.trees(i).topNode.impurity
      beforeSaveModel.model.trees(i).topNode.id shouldEqual afterSaveModel.model.trees(i).topNode.id
      beforeSaveModel.model.trees(i).topNode.stats.get.gain shouldEqual afterSaveModel.model.trees(i).topNode.stats.get.gain
    }
    beforeSaveModel.model.treeWeights shouldEqual afterSaveModel.model.treeWeights
  }

}