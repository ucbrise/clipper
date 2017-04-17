package ai.clipper.container

import org.json4s._
import org.json4s.jackson.JsonMethods._

import scala.reflect.runtime.universe
import scala.reflect.runtime.universe.TypeTag
import scala.reflect.api
import java.io._
import scala.io.Source

import org.apache.spark.{SparkConf, SparkContext}
import org.apache.spark.mllib.tree.model.DecisionTreeModel
import org.apache.spark.mllib.tree.DecisionTree
import org.apache.spark.mllib.util.MLUtils
import org.apache.spark.mllib.linalg.Vector
import org.apache.spark.mllib.util.Saveable
import org.apache.spark.mllib.classification.{LogisticRegressionModel, LogisticRegressionWithLBFGS}
import org.apache.spark.mllib.classification.NaiveBayesModel
import org.apache.spark.mllib.classification.SVMModel
import org.apache.spark.mllib.clustering.BisectingKMeansModel

sealed abstract class MLlibModel {
  def predict(features: Vector): Double
  def save(sc: SparkContext, path: String): Unit
}

// LogisticRegressionModel
case class MLlibLogisticRegressionModel(model: LogisticRegressionModel) extends MLlibModel {
  override def predict(features: Vector): Double  = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// NaiveBayesModel
case class MLlibNaiveBayesModel(model: NaiveBayesModel) extends MLlibModel {
  override def predict(features: Vector): Double  = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}


// SVMModel
case class MLlibSVMModel(model: SVMModel) extends MLlibModel {
  override def predict(features: Vector): Double  = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}


// BisectingKMeansModel
case class MLlibBisectingKMeansModel(model: BisectingKMeansModel) extends MLlibModel {
  override def predict(features: Vector): Double  = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

abstract class Container {

  def init(sc: SparkContext, model: MLlibModel): Unit

  def predict(xs: Vector) : Double

}

object MLlibLoader {
  def metadataPath(path: String): String = s"$path/metadata"

  def getModelClassName(sc: SparkContext, path: String) = {
    val str = sc.textFile(metadataPath(path)).take(1)(0)
    println(s"JSON STRING: $str")
    val json = parse(str)
    val JString(className) = (json \ "class")
    className
  }

  def load(sc: SparkContext, path: String): MLlibModel = {
    val className = getModelClassName(sc, path)
    // Reflection Code
    val mirror = universe.runtimeMirror(getClass.getClassLoader)
    val modelModule = mirror.staticModule(className)
    val anyInst = mirror.reflectModule(modelModule).instance
    val loader = anyInst.asInstanceOf[org.apache.spark.mllib.util.Loader[_]]
    val model = loader.load(sc, path) match {
      case model: LogisticRegressionModel => MLlibLogisticRegressionModel(model)
      case model: NaiveBayesModel => MLlibNaiveBayesModel(model)
      case model: SVMModel => MLlibSVMModel(model)
      case model: BisectingKMeansModel => MLlibBisectingKMeansModel(model)
    }
    model
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
