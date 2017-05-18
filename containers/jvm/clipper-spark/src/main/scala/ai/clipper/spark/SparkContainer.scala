package ai.clipper.spark

import java.util.ArrayList

import ai.clipper.container.ClipperModel
import ai.clipper.container.data.{DataType, DoubleVector, SerializableString}
import org.apache.spark.SparkContext
import org.apache.spark.ml.PipelineModel
import org.apache.spark.mllib.classification.{LogisticRegressionModel, NaiveBayesModel, SVMModel}
import org.apache.spark.mllib.clustering.{BisectingKMeansModel, GaussianMixtureModel, KMeansModel}
import org.apache.spark.mllib.linalg.{Vector, Vectors}
import org.apache.spark.mllib.recommendation.MatrixFactorizationModel
import org.apache.spark.mllib.regression.{IsotonicRegressionModel, LassoModel, LinearRegressionModel, RidgeRegressionModel}
import org.apache.spark.mllib.tree.model.{DecisionTreeModel, GradientBoostedTreesModel, RandomForestModel}
import org.json4s._
import org.json4s.jackson.JsonMethods._

import scala.collection.JavaConversions._
import scala.reflect.runtime.universe

abstract class SparkModelContainer extends ClipperModel[DoubleVector] {

  override def getInputType : DataType = {
    DataType.Doubles
  }

  final override def predict(inputVectors: ArrayList[DoubleVector]): ArrayList[SerializableString] = {
    val inputs = inputVectors.map(x => Vectors.dense(new Array[Double](x.getData.remaining()))).toList
    new ArrayList(predict(inputs).map(x => new SerializableString(x.toString)))
  }

  def predict(x: List[Vector]): List[Float]

}

abstract class MLlibContainer extends SparkModelContainer {

  def init(sc: SparkContext, model: MLlibModel): Unit

  override def predict(xs: List[Vector]): List[Float]

}
abstract class PipelineModelContainer extends SparkModelContainer {

  def init(sc: SparkContext, model: PipelineModel): Unit

  override def predict(xs: List[Vector]): List[Float]

}

sealed abstract class MLlibModel {
  def predict(features: Vector): Double
  def save(sc: SparkContext, path: String): Unit
}

// Classification

// LogisticRegressionModel
case class MLlibLogisticRegressionModel(model: LogisticRegressionModel)
    extends MLlibModel {
  override def predict(features: Vector): Double = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// SVMModel
case class MLlibSVMModel(model: SVMModel) extends MLlibModel {
  override def predict(features: Vector): Double = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// NaiveBayesModel
case class MLlibNaiveBayesModel(model: NaiveBayesModel) extends MLlibModel {
  override def predict(features: Vector): Double = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// Clustering

// BisectingKMeansModel
case class MLlibBisectingKMeansModel(model: BisectingKMeansModel)
    extends MLlibModel {
  override def predict(features: Vector): Double = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// GaussianMixtureModel
case class MLlibGaussianMixtureModel(model: GaussianMixtureModel)
    extends MLlibModel {
  override def predict(features: Vector): Double = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// KMeansModel
case class MLlibKMeansModel(model: KMeansModel) extends MLlibModel {
  override def predict(features: Vector): Double = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// // LDAModel
// case class MLlibLDAModel(model: LDAModel) extends MLlibModel {
//   override def predict(features: Vector): Double  = {
//     model.predict(features)
//   }
//
//   override def save(sc: SparkContext, path: String): Unit = {
//     model.save(sc, path)
//   }
// }
//
// // PowerIterationClusteringModel
// case class MLlibPowerIterationClusteringModel(model: PowerIterationClusteringModel) extends MLlibModel {
//   override def predict(features: Vector): Double  = {
//     model.predict(features)
//   }
//
//   override def save(sc: SparkContext, path: String): Unit = {
//     model.save(sc, path)
//   }
// }
//
// // Features
//
// // ChiSqSelectorModel
// case class MLlibChiSqSelectorModel(model: ChiSqSelectorModel) extends MLlibModel {
//   override def predict(features: Vector): Double  = {
//     model.predict(features)
//   }
//
//   override def save(sc: SparkContext, path: String): Unit = {
//     model.save(sc, path)
//   }
// }
//
// // Word2VecModel
// case class MLlibWord2VecModel(model: Word2VecModel) extends MLlibModel {
//   override def predict(features: Vector): Double  = {
//     model.predict(features)
//   }
//
//   override def save(sc: SparkContext, path: String): Unit = {
//     model.save(sc, path)
//   }
// }
//
// // FPM
//
// // FPGrowthModel
// case class MLlibFPGrowthModel(model: FPGrowthModel) extends MLlibModel {
//   override def predict(features: Vector): Double  = {
//     model.predict(features)
//   }
//
//   override def save(sc: SparkContext, path: String): Unit = {
//     model.save(sc, path)
//   }
// }
//
// // PrefixSpanModel
// case class MLlibPrefixSpanModel(model: PrefixSpanModel) extends MLlibModel {
//   override def predict(features: Vector): Double  = {
//     model.predict(features)
//   }
//
//   override def save(sc: SparkContext, path: String): Unit = {
//     model.save(sc, path)
//   }
// }

//Recommendation

// MatrixFactorizationModel
case class MLlibMatrixFactorizationModel(model: MatrixFactorizationModel)
    extends MLlibModel {
  override def predict(features: Vector): Double = {
    val userId = features(0).toInt
    val productId = features(1).toInt
    model.predict(userId, productId)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// Regression

// IsotonicRegressionModel
case class MLlibIsotonicRegressionModel(model: IsotonicRegressionModel)
    extends MLlibModel {
  override def predict(features: Vector): Double = {
    model.predict(features(0))
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// LassoModel
case class MLlibLassoModel(model: LassoModel) extends MLlibModel {
  override def predict(features: Vector): Double = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// LinearRegressionModel
case class MLlibLinearRegressionModel(model: LinearRegressionModel)
    extends MLlibModel {
  override def predict(features: Vector): Double = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// RidgeRegressionModel
case class MLlibRidgeRegressionModel(model: RidgeRegressionModel)
    extends MLlibModel {
  override def predict(features: Vector): Double = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// Tree

// DecisionTreeModel
case class MLlibDecisionTreeModel(model: DecisionTreeModel)
    extends MLlibModel {
  override def predict(features: Vector): Double = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// RandomForestModel
case class MLlibRandomForestModel(model: RandomForestModel)
    extends MLlibModel {
  override def predict(features: Vector): Double = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

// GradientBoostedTreesModel
case class MLlibGradientBoostedTreesModel(model: GradientBoostedTreesModel)
    extends MLlibModel {
  override def predict(features: Vector): Double = {
    model.predict(features)
  }

  override def save(sc: SparkContext, path: String): Unit = {
    model.save(sc, path)
  }
}

object MLlibLoader {
  def metadataPath(path: String): String = s"$path/metadata"

  def getModelClassName(sc: SparkContext, path: String): String = {
//    val str = sc.textFile(metadataPath(path)).take(1)(0)
    val str = sc.textFile(metadataPath(path)).collect()
    println(s"ALL METADATA:")
    str.foreach(println(_))
    val jsonStr = str.take(1)(0)
    println(s"JSON STRING: $jsonStr")
    val json = parse(jsonStr)
    val JString(className) = (json \ "class")
    // Spark hardcoded the class name for DecisionTreeModel for some reason,
    // then changed the package. We substitute the correct class name.
    if (className == "org.apache.spark.mllib.tree.DecisionTreeModel") {
      "org.apache.spark.mllib.tree.model.DecisionTreeModel"
    } else {
      className
    }
  }

  def load(sc: SparkContext, path: String): MLlibModel = {
    val className = getModelClassName(sc, path)
    // Reflection Code
    val mirror = universe.runtimeMirror(getClass.getClassLoader)
    val modelModule = mirror.staticModule(className)
    val anyInst = mirror.reflectModule(modelModule).instance
    val loader = anyInst.asInstanceOf[org.apache.spark.mllib.util.Loader[_]]
    val model = loader.load(sc, path) match {
      case model: LogisticRegressionModel =>
        MLlibLogisticRegressionModel(model)
      case model: NaiveBayesModel => MLlibNaiveBayesModel(model)
      case model: SVMModel => MLlibSVMModel(model)
      case model: BisectingKMeansModel => MLlibBisectingKMeansModel(model)
      case model: GaussianMixtureModel => MLlibGaussianMixtureModel(model)
      case model: KMeansModel => MLlibKMeansModel(model)
      // case model: LDAModel => MLlibLDAModel(model)
      // case model: PowerIterationClusteringModel => MLlibPowerIterationClusteringModel(model)
      // case model: ChiSqSelectorModel => MLlibChiSqSelectorModel(model)
      // case model: Word2VecModel => MLlibWord2VecModel(model)
      // case model: FPGrowthModel => MLlibFPGrowthModel(model)
      // case model: PrefixSpanModel => MLlibPrefixSpanModel(model)
      case model: MatrixFactorizationModel =>
        MLlibMatrixFactorizationModel(model)
      case model: IsotonicRegressionModel =>
        MLlibIsotonicRegressionModel(model)
      case model: LassoModel => MLlibLassoModel(model)
      case model: LinearRegressionModel => MLlibLinearRegressionModel(model)
      case model: RidgeRegressionModel => MLlibRidgeRegressionModel(model)
      case model: DecisionTreeModel => MLlibDecisionTreeModel(model)
      case model: RandomForestModel => MLlibRandomForestModel(model)
      case model: GradientBoostedTreesModel =>
        MLlibGradientBoostedTreesModel(model)
    }
    model
  }
}
