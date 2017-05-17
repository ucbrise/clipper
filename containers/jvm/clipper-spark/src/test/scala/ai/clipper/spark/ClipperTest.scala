package ai.clipper.spark

import java.net.URLClassLoader
import java.nio.file.StandardCopyOption.REPLACE_EXISTING
import java.nio.file.StandardOpenOption.CREATE
import java.nio.file.{Files, Paths}
import java.nio.charset.Charset

import org.apache.commons.io.FileUtils
import org.apache.spark.SparkContext
import org.apache.spark.ml.PipelineModel
import org.json4s._
import org.json4s.jackson.Serialization.{read, write}
import org.json4s.jackson.Serialization
import org.scalatest.{BeforeAndAfter, FunSuite}


import scalaj.http._

import scala.sys.process._
