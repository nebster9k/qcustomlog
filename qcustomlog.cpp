/**
 * @file qcustomlog.cpp
 * @brief A class for handling logging operations
 * @details This class provides static methods for logging messages, compatible with qInstallMessageHandler to handle Qt messages
 *
 * @attention Ensure that the QCustomLog class is properly initialized before using logging from multiple threads
 *
 * @details This code is released under the MIT license
 * @copyright (c) 2025 Dmitrii Permiakov [nebster9k]
 *
 * @see preproc @see qcustomlog.h
 */

#include <qcustomlog.h>

bool QCustomLog::setTimestampFormat(const QString& format)
{
   if(format.isEmpty()) return false;
   if(!QDateTime::fromString(QDateTime::currentDateTime().toString(format),format).isValid()) return false;

   m_logMessageFormat="'['"+format+"']'";
   return true;
}

bool QCustomLog::initLogging(QString logDir, quint32 flushTime, quint32 maxFiles, quint32 maxFileSize)
{
   if(!logDir.isEmpty()) QCustomLog::normalizePath(logDir); else logDir=QCoreApplication::applicationDirPath()+"/";
   if(!QCustomLog::ensureDirectoryWritable(logDir))
   {
      QCustomLog::callErrorHandler("Log directory is not writable");
      return false;
   }
   m_logDir.setPath(logDir);

   // first-time log file creation or rotation
   if(!QCustomLog::rotateLogFiles(m_logFileName)) return false;

   if(maxFiles<2) m_maxLogFiles=2; else m_maxLogFiles=maxFiles;
   if(maxFileSize<(100*1024)) m_maxLogFileSize=(100*1024); else m_maxLogFileSize=maxFileSize;

   if(flushTime>=1000)
   {
      m_logBufferEnabled=true;
      m_logBufferTimer.setInterval(flushTime); m_logBufferTimer.setSingleShot(true);
      QObject::connect(&m_logBufferTimer,&QTimer::timeout,qOverload<>(&QCustomLog::flushBuffer));
   } else m_logBufferEnabled=false;

   qInstallMessageHandler(QCustomLog::messageHandler);

   if(m_logBufferEnabled) QCustomLog::m_logBufferTimer.start();

   return true;
}

void QCustomLog::messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
   QDateTime now=m_utcMode ? QDateTime::currentDateTimeUtc() : QDateTime::currentDateTime();
   QString message; QString category=QString(context.category);

   #ifdef NDEBUG
      if(type==QtMsgType::QtDebugMsg) return;
   #endif

   if(type==QtMsgType::QtDebugMsg)
   {
      QString func=context.function;
      if(func.indexOf("virtual ")==0) func.remove("virtual ");
      func.remove(func.indexOf('(')+1,func.lastIndexOf(')')-func.indexOf('(')-1);
      message=QString(context.file).remove(0,qMax(QString(context.file).lastIndexOf("\\"),QString(context.file).lastIndexOf("/"))+1)+": "+func+": "+msg;
   } else message=msg;

   // slightly spaghettified for performance
   QString formattedMessage=now.toString(m_logMessageFormat);
   switch(type)
   {
      case QtMsgType::QtInfoMsg:
         formattedMessage.append(" [INF] ["+category+"] "+message);
         if(m_cleanLogCategory.isEmpty())
         {
            if(QCustomLog::levelGreaterOrEqual(type,m_minOutLevel)) qInfo().noquote() << formattedMessage;
         } else if(category==m_cleanLogCategory) qInfo().noquote() << msg;
         break;
      case QtMsgType::QtWarningMsg:
         formattedMessage.append(" [WRN] ["+category+"] "+message);
         if(m_cleanLogCategory.isEmpty())
         {
            if(QCustomLog::levelGreaterOrEqual(type,m_minOutLevel)) qWarning().noquote() << "\033[33m"+formattedMessage+"\033[0m";
         } else if(category==m_cleanLogCategory) qWarning().noquote() << msg;
         break;
      case QtMsgType::QtCriticalMsg:
         formattedMessage.append(" [CRT] ["+category+"] "+message);
         if(m_cleanLogCategory.isEmpty())
         {
            if(QCustomLog::levelGreaterOrEqual(type,m_minOutLevel)) qCritical().noquote() << "\033[31m"+formattedMessage+"\033[0m";
         } else if(category==m_cleanLogCategory) qCritical().noquote() << msg;
         break;
      case QtMsgType::QtFatalMsg:
         formattedMessage.append(" [FTL] ["+category+"] "+message);

         // must not write or transmit potentially sensitive information when prohibited, even at fatal levels
         if(m_cleanLogCategory.isEmpty() || category!=m_cleanLogCategory || m_cleanToFile)
         {
            m_logBufferMutex.lock();
            m_logBuffer.enqueue(formattedMessage);
            m_logBufferMutex.unlock();
            QCustomLog::flushBuffer(true);

            m_customHandlerMutex.lock();
            QCustomLog::instance().sendLog(now,type,category,message);
            m_customHandlerMutex.unlock();
         }

         // fatal level implies that it is better to get something than to miss something due to keeping a clean output
         #if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
            if(m_cleanLogCategory.isEmpty())
            {
               qFatal().noquote() << "\033[35m"+formattedMessage+"\033[0m";
            } else qFatal().noquote() << "[FTL] "+msg;
         #else
            #ifdef __GNUC__
               #pragma GCC diagnostic push
               #pragma GCC diagnostic ignored "-Wformat-security"
            #endif
            if(m_cleanLogCategory.isEmpty())
            {
               qFatal(QString("\033[35m"+formattedMessage+"\033[0m").toUtf8().constData());
            } else qFatal(QString("[FTL] "+msg).toUtf8().constData());
            #ifdef __GNUC__
               #pragma GCC diagnostic pop
            #endif
         #endif
         break;
      default: // QtMsgType::QtDebugMsg
         formattedMessage.append(" [DBG] ["+category+"] "+message);

         if(m_cleanLogCategory.isEmpty())
         {
            if(QCustomLog::levelGreaterOrEqual(type,m_minOutLevel)) qDebug().noquote() << "\033[90m"+formattedMessage+"\033[0m";
         } else if(category==m_cleanLogCategory) qDebug().noquote() << msg;
         break;
   }

   // to avoid double logging, and also in case of a fatal log it usually doesn't get here because of the core dump
   if(type==QtMsgType::QtFatalMsg) return;

   // must not write or transmit potentially sensitive information when prohibited
   if(m_cleanLogCategory.isEmpty() || category!=m_cleanLogCategory || m_cleanToFile)
   {
      if(QCustomLog::levelGreaterOrEqual(type,m_minOutFileLevel))
      {
         m_logBufferMutex.lock();
         m_logBuffer.enqueue(formattedMessage);
         m_logBufferMutex.unlock();

         if(type==QtMsgType::QtCriticalMsg) QCustomLog::flushBuffer(true);
         else if(!m_logBufferEnabled) QCustomLog::flushBuffer(false);
      }

      m_customHandlerMutex.lock();
      QCustomLog::instance().sendLog(now,type,category,message);
      m_customHandlerMutex.unlock();
   }
}

void QCustomLog::flushBuffer(bool force)
{
   if(m_logBufferEnabled) QMetaObject::invokeMethod(&QCustomLog::m_logBufferTimer,qOverload<>(&QTimer::start),Qt::QueuedConnection);

   m_logBufferMutex.lock();
   if(m_logBuffer.isEmpty()) { m_logBufferMutex.unlock(); return; }

   if(m_logBufferEnabled && m_logBuffer.count()>m_maxBufferMessages)
   {
      m_maxBufferMessages=m_logBuffer.count();
      m_logBuffer.reserve(m_maxBufferMessages);
   }

   // double buffer to avoid blocking the main buffer for a long time
   // because levels below critical do not cause immediate buffer flushing and their operation will not be slowed down
   QQueue<QString> doubleBuffer=m_logBuffer;
   m_logBuffer.clear();
   m_logBufferMutex.unlock();

   m_logFileMutex.lock();
   if(!QCustomLog::rotateLogFiles(m_logFileName))
   {
      m_logFileMutex.unlock();

      // extremely rare situation, but it will potentially helps to avoid losing some of logs
      m_logBufferMutex.lock();
      doubleBuffer.append(m_logBuffer);
      m_logBuffer=doubleBuffer;
      m_logBufferMutex.unlock();

      return;
   }

   QFile logFile(m_logDir.absoluteFilePath(m_logFileName));

   QElapsedTimer elapsedTimer; elapsedTimer.start();
   if(!logFile.open(QFile::OpenModeFlag::Text|QFile::OpenModeFlag::WriteOnly|QFile::OpenModeFlag::Append))
   {
      QCustomLog::callErrorHandler("Log file \""+m_logFileName+"\" open error: "+logFile.errorString());
      m_logFileMutex.unlock();

      // extremely rare situation, but it will potentially helps to avoid losing some of logs
      m_logBufferMutex.lock();
      doubleBuffer.append(m_logBuffer);
      m_logBuffer=doubleBuffer;
      m_logBufferMutex.unlock();

      return;
   }

   while(!doubleBuffer.isEmpty()) logFile.write(doubleBuffer.dequeue().toUtf8()+'\n');
   if(force) logFile.flush();

   logFile.close();
   float elapsed=(float)elapsedTimer.nsecsElapsed()/1e9; // in seconds

   m_logFileMutex.unlock();

   // calculate EMA (Exponential Moving Average) for buffer flush time with alpha=0.1
   float elapsedAvg=m_logBufferFlushTime;
   if(elapsedAvg<=+0.0f) elapsedAvg=elapsed; else elapsedAvg=(elapsedAvg*0.9f)+(elapsed*0.1f);
   m_logBufferFlushTime=elapsedAvg;

   #ifndef NDEBUG
      if(m_minOutLevel==QtMsgType::QtDebugMsg && !m_cleanLogCategoryIsSet)
         std::cout << "--- Log buffer flushed in " << elapsed*1e3 << " ms (EMA: " << elapsedAvg*1e3 << " ms)" << std::endl;
   #endif
}

void QCustomLog::callErrorHandler(const QString& msg)
{
   if(m_errorHandler) // safe because of requirement to set the error handler before using logging
   {
      M_errorHandlerMutex.lock();
      m_errorHandler(msg);
      M_errorHandlerMutex.unlock();
   }
}

bool QCustomLog::ensureDirectoryWritable(const QString& dirPath)
{
   QDir dir(dirPath);
   if(!dir.exists() && !dir.mkpath(dirPath)) return false;
   if(!dir.isReadable()) return false;

   QFile testFile(dirPath+"test.tmp");
   if(testFile.exists() && !testFile.remove()) return false;
   if(!testFile.open(QFile::OpenModeFlag::WriteOnly|QFile::OpenModeFlag::Truncate)) return false;
   testFile.close();
   if(!testFile.remove()) return false;
   return true;
}

void QCustomLog::normalizePath(QString& path)
{
   path.replace('\\','/');
   path.replace(QRegularExpression("\\/{2,}"),"/");
   if(!path.endsWith('/')) path.append('/');
}

bool QCustomLog::rotateLogFiles(QString& logFileName)
{
   if(m_logDir.path().isEmpty())
   {
      QCustomLog::callErrorHandler("Log directory is not set");
      return false;
   }

   static QString mainLogFileName=QCoreApplication::applicationName()+"_0.log";
   static bool firstTime=true;

   QElapsedTimer elapsedTimer; elapsedTimer.start();

   // check existing log file size
   if(!logFileName.isEmpty())
   {
      if(logFileName==mainLogFileName)
      {
         QFileInfo logFileInfo(m_logDir.absoluteFilePath(logFileName));
         if(!logFileInfo.exists() || logFileInfo.size()>=m_maxLogFileSize) logFileName.clear();
      } else logFileName.clear();
   }

   if(logFileName.isEmpty())
   {
      QFileInfoList fileList=m_logDir.entryInfoList({QCoreApplication::applicationName()+"_*.log"},QDir::Files);

      // filter non-number postfixes
      for(int i=fileList.count()-1;i>=0;i--)
      {
         QString fileName=fileList.at(i).fileName();
         QString postfix=fileName.mid(fileName.indexOf('_')+1,fileName.lastIndexOf('.')-fileName.indexOf('_')-1);
         bool ok; postfix.toUInt(&ok);
         if(!ok)
         {
            if(!QFile::remove(fileList.at(i).absoluteFilePath())) callErrorHandler("Unknown log file \""+fileList.at(i).fileName()+"\" deletion error");
            fileList.removeAt(i);
         }
      }

      if(!fileList.isEmpty())
      {
         // sort by postfix numerically
         std::sort(fileList.begin(),fileList.end(),[](const QFileInfo& a, const QFileInfo& b)
         {
            quint32 aPostfix=a.fileName().mid(a.fileName().indexOf('_')+1,a.fileName().lastIndexOf('.')-a.fileName().indexOf('_')-1).toUInt();
            quint32 bPostfix=b.fileName().mid(b.fileName().indexOf('_')+1,b.fileName().lastIndexOf('.')-b.fileName().indexOf('_')-1).toUInt();

            return aPostfix<bPostfix;
         });

         // remove exactly redundant log files
         while(fileList.count()>m_maxLogFiles)
         {
            if(!QFile::remove(fileList.last().absoluteFilePath())) callErrorHandler("Log file \""+fileList.last().fileName()+"\" deletion error");
            fileList.removeLast();
         }

         // new file is needed
         if(fileList.first().size()>=m_maxLogFileSize || fileList.first().fileName()!=mainLogFileName)
         {
            if(fileList.count()>=m_maxLogFiles) // ensure that after creation the number of log files will not exceed the limit
            {
               if(!QFile::remove(fileList.last().absoluteFilePath())) callErrorHandler("Log file \""+fileList.last().fileName()+"\" deletion error");
               fileList.removeLast();
            }

            // check for obstacles to linear renaming
            bool obstacleFound=false; QString lastFileName=QCoreApplication::applicationName()+"_"+QString::number(fileList.count())+".log";
            for(const auto& fileInfo:fileList)
            {
               if(fileInfo.fileName()==lastFileName) { obstacleFound=true; break; }
            }

            // temporary rename to avoid names obstacles
            if(obstacleFound)
            {
               for(auto& fileInfo:fileList)
               {
                  if(!QFile::rename(fileInfo.absoluteFilePath(),m_logDir.absolutePath()+"/"+fileInfo.fileName()+".temp"))
                  {
                     callErrorHandler("Log file \""+fileInfo.fileName()+"\" renaming error");
                     continue; // even with rotation issues, we can still write logs to the main file, it's better than not flushing
                  }
                  fileInfo.setFile(m_logDir.absolutePath()+"/"+fileInfo.fileName()+".temp");
               }
            }

            // linear rename
            for(int i=fileList.count()-1;i>=0;i--)
            {
               if(fileList.at(i).fileName()==QCoreApplication::applicationName()+"_"+QString::number(i+1)+".log") continue;

               if(!QFile::rename(fileList.at(i).absoluteFilePath(),m_logDir.absolutePath()+"/"+QCoreApplication::applicationName()+"_"+QString::number(i+1)+".log"))
               {
                  callErrorHandler("Log file \""+fileList.at(i).fileName()+"\" renaming error");
                  continue; // even with rotation issues, we can still write logs to the main file, it's better than not flushing
               }
               fileList[i].setFile(m_logDir.absolutePath()+"/"+QCoreApplication::applicationName()+"_"+QString::number(i+1)+".log");
            }

            // create empty main log file
            if(!QCustomLog::logFileTouch(mainLogFileName)) { logFileName=mainLogFileName; return false; }
         }
      } else if(!QCustomLog::logFileTouch(mainLogFileName)) { logFileName=mainLogFileName; return false; }
   }

   float elapsed=(float)elapsedTimer.nsecsElapsed()/1e9; // in seconds


   // calculate EMA (Exponential Moving Average) of log files rotation time with alpha=0.2
   float elapsedAvg=m_logRotationTime;
   if(!firstTime) // skip the first call in calculating the average duration,
   {              // since the files are most likely to be affected in it, and the performance of the initial call is usually not important
      if(elapsedAvg<=+0.0f) elapsedAvg=elapsed; else elapsedAvg=(elapsedAvg*0.8f)+(elapsed*0.2f);
      m_logRotationTime=elapsedAvg;
   }

   #ifndef NDEBUG // first call will be inside init and most likely before the clean category is installed, so it should be skipped
      if(m_minOutLevel==QtMsgType::QtDebugMsg && !m_cleanLogCategoryIsSet && !firstTime)
         std::cout << "--- Log files rotate time: " << elapsed*1e3 << " ms (EMA: " << elapsedAvg*1e3 << " ms)" << std::endl;
   #endif

   if(firstTime) firstTime=false; // with a check because only in the first call multithreaded recording does not occur

   logFileName=mainLogFileName;
   return true;
}

bool QCustomLog::logFileTouch(const QString& fileName)
{
   QFile newLogFile(m_logDir.absolutePath()+"/"+fileName);
   if(!newLogFile.open(QFile::OpenModeFlag::WriteOnly|QFile::OpenModeFlag::Truncate))
   {
      callErrorHandler("Log file \""+fileName+"\" creation error");
      return false;
   }
   newLogFile.close();
   return true;
}

bool QCustomLog::levelGreaterOrEqual(QtMsgType level, QtMsgType minLevel)
{
   // this is necessary because different versions of qt have different order of QtMsgType enum

   if(level==minLevel) return true;

   switch(minLevel)
   {
      case QtMsgType::QtDebugMsg:
         return true; // all levels are greater or equal to debug
      case QtMsgType::QtInfoMsg:
         if(level==QtMsgType::QtDebugMsg) return false; else return true;
         break;
      case QtMsgType::QtWarningMsg:
         if(level==QtMsgType::QtDebugMsg || level==QtMsgType::QtInfoMsg) return false; else return true;
         break;
      case QtMsgType::QtCriticalMsg:
         if(level==QtMsgType::QtCriticalMsg || level==QtMsgType::QtFatalMsg) return true; else return false;
         break;
      case QtMsgType::QtFatalMsg:
         if(level==QtMsgType::QtFatalMsg) return true; else return false;
         break;
      default: return true; // unknown level, log it just in case
   }
}
