package ai.clipper

import scala.reflect.runtime.universe
import scala.reflect.runtime.universe.TypeTag
import scala.reflect.api
import scala.io.Source
import java.nio.file.StandardCopyOption.REPLACE_EXISTING
import java.nio.file.StandardOpenOption.CREATE
import java.nio.file.{Files,Paths,Path}
import java.net.URL
import java.net.URLClassLoader
import java.lang.ClassLoader

import org.apache.spark.{SparkContext, SparkConf}

import org.json4s._
import org.json4s.jackson.JsonMethods._

import ai.clipper.container.{MLlibModel,MLlibLoader,Container}


object Clipper {

  // TODO: also try serializing a container instance?
  def deployModel(sc: SparkContext,
                  name: String,
                  version: Int,
                  model: MLlibModel,
                  containerClass: String): Unit = {
    val path = s"/tmp/$name/$version/"
    model.save(sc, path)
    // val containerClass = container.getClass.getName
    Files.write(Paths.get(s"$path/container.txt"), s"$containerClass\n".getBytes, CREATE)
    val jarPath = Paths.get(getClass.getProtectionDomain.getCodeSource.getLocation.getPath)
    println(s"JAR path: $jarPath")
    System.out.flush()
    Files.copy(jarPath, Paths.get(s"$path/container.jar"), REPLACE_EXISTING)

  }

  def loadModel(sc: SparkContext,
                path: String,
                containerClass: String) : Container = {
    val model = MLlibLoader.load(sc, path)
    val jarPath = Paths.get(s"$path/container.jar")
    val classLoader = new URLClassLoader(Array(jarPath.toUri.toURL), getClass.getClassLoader)
    val container: Container = constructContainer(classLoader, containerClass).get
    container.init(sc, model)
    container
  }

  private def selectConstructor(symbol: universe.Symbol) = {
      val constructors = symbol.typeSignature.members.filter(_.isConstructor).toList
      if (constructors.length > 1) println(
             s"""Warning: $symbol has several constructors, arbitrarily picking the first one: 
                |         ${constructors.mkString("\n         ")}""".stripMargin)
      constructors.head.asMethod
    }

  // adapted from http://stackoverflow.com/q/34227984/814642
  def constructContainer(classLoader: ClassLoader, containerClass: String) : Option[Container] = {
    val clazz = classLoader.loadClass(containerClass)
    val runtimeMirror: universe.Mirror = universe.runtimeMirror(classLoader)
    val classToLoad = Class.forName(containerClass, true, classLoader)
    // val classSymbol: universe.ClassSymbol = runtimeMirror.classSymbol(Class.forName(containerClass))
    val classSymbol: universe.ClassSymbol = runtimeMirror.classSymbol(classToLoad)
    val classMirror: universe.ClassMirror = runtimeMirror.reflectClass(classSymbol)
    val constructorMirror = classMirror.reflectConstructor(selectConstructor(classSymbol))
    try {
      return Some(constructorMirror().asInstanceOf[Container])
    } catch {
      case wrongClass: ClassCastException => println(s"Could not cast provided class to Container: $wrongClass")
      case e: Throwable => println(s"Error loading constructor: $e")
    }
    None
  }
}
