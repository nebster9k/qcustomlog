/**
 * @file qcustomlog.h
 * @brief A class for handling logging operations
 * @details This class provides static methods for logging messages, compatible with qInstallMessageHandler to handle Qt messages
 *
 * @attention Ensure that the QCustomLog class is properly initialized before using logging from multiple threads
 *
 * @details This code is released under the MIT license
 * @copyright (c) 2025 Dmitrii Permiakov [nebster9k]
 *
 * @see preproc @see qcustomlog.cpp
 */

#ifndef QCUSTOMLOG_H
#define QCUSTOMLOG_H

#include <QDateTime>
#include <QMetaObject>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <atomic>
#include <QString>
#include <QDir>
#include <QFile>
#include <QQueue>
#include <QTimer>
#include <QMutex>
#include <QDebug>

#ifndef NDEBUG
   #include <iostream>
#endif

#define _qclog_GET_MACRO(_1,_2,NAME,...) NAME

/**
 * @brief Log debug message macro
 * @details Log debug message with or without category
 * @param ... Message and optional category
 * @details If NDEBUG is defined, then debug messages will not be processed because of performance reasons
 * @attention This macro requires _qclog_category with the category name to be defined before use
 */
#define logDebug(...)                  _qclog_GET_MACRO(__VA_ARGS__,_qclog_logDebugWCat,_qclog_logDebug)(__VA_ARGS__)
#ifndef NDEBUG
   #define _qclog_logDebug(x)          qDebug(QLoggingCategory(_qclog_category)).noquote() << x
   #define _qclog_logDebugWCat(x,c)    qDebug(QLoggingCategory(c)).noquote() << x
#else
   #define _qclog_logDebug(x)          ((void)0)
   #define _qclog_logDebugWCat(x,c)    ((void)0)
#endif

/**
 * @brief Log information message macro
 * @details Log information message with or without category
 * @param ... Message and optional category
 * @attention This macro requires _qclog_category with the category name to be defined before use
 */
#define logInfo(...)                   _qclog_GET_MACRO(__VA_ARGS__,_qclog_logInfoWCat,_qclog_logInfo)(__VA_ARGS__)
#define _qclog_logInfo(x)              qInfo(QLoggingCategory(_qclog_category)).noquote() << x
#define _qclog_logInfoWCat(x,c)        qInfo(QLoggingCategory(c)).noquote() << x

/**
 * @brief Log warning message macro
 * @details Log warning message with or without category
 * @param ... Message and optional category
 * @attention This macro requires _qclog_category with the category name to be defined before use
 */
#define logWarning(...)                _qclog_GET_MACRO(__VA_ARGS__,_qclog_logWarningWCat,_qclog_logWarning)(__VA_ARGS__)
#define _qclog_logWarning(x)           qWarning(QLoggingCategory(_qclog_category)).noquote() << x
#define _qclog_logWarningWCat(x,c)     qWarning(QLoggingCategory(c)).noquote() << x

/**
 * @brief Log critical message macro
 * @details Log critical message with or without category
 * @param ... Message and optional category
 * @attention This macro requires _qclog_category with the category name to be defined before use
 */
#define logCritical(...)               _qclog_GET_MACRO(__VA_ARGS__,_qclog_logCriticalWCat,_qclog_logCritical)(__VA_ARGS__)
#define _qclog_logCritical(x)          qCritical(QLoggingCategory(_qclog_category)).noquote() << x
#define _qclog_logCriticalWCat(x,c)    qCritical(QLoggingCategory(c)).noquote() << x

/**
 * @brief Log fatal message macro
 * @details Log fatal message with or without category
 * @param ... Message and optional category
 * @details Categories on fatal messages are supported only on Qt 6.5.0 and higher
 * @attention This macro requires _qclog_category with the category name to be defined before use
 * @attention Fatal messages cause the application to exit immediately after the message is logged because of the automatic core dump
 */
#define logFatal(...)                  _qclog_GET_MACRO(__VA_ARGS__,_qclog_logFatalWCat,_qclog_logFatal)(__VA_ARGS__)
#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
   #define _qclog_logFatal(x)          qFatal(QLoggingCategory(_qclog_category)).noquote() << x
   #define _qclog_logFatalWCat(x,c)    qFatal(QLoggingCategory(c)).noquote() << x
#else
   #define _qclog_logFatal(x)          qFatal(x)
   #define _qclog_logFatalWCat(x,c)    qFatal(x)
#endif

class QCustomLog
{
   public:
      using ErrorHandler=void (*)(const QString&); /**< Error handler type */

      /**
       * @brief Set custom log instance
       * @details Custom log instance is used to override a @see sendLog() function, for example to send somewhere like a database
       * @param instance Custom log instance pointer
       * @attention Call this method before creating threads and starting the application event loop
       */
      static void setInstance(QCustomLog* instance) { m_customInstance=instance; }

      /**
       * @brief Set error handler
       * @details Error handler will be called in case of logging errors, useful for debugging or force closing the application
       * @param handler Error handler function pointer
       * @attention Call this method before creating threads and starting the application event loop
       */
      static void setErrorHandler(ErrorHandler handler) { m_errorHandler=handler; }

      /**
       * @brief Set logs timestamp format
       * @details Set format of timestamp in the log messages, default is "yyyy.MM.dd HH:mm:ss.zzz"
       * @param format Timestamp format string
       * @return Result of the operation
       * @retval true Timestamp format was set successfully
       * @retval false Timestamp format was not set, e.g. invalid format string
       */
      static bool setTimestampFormat(const QString& format);

      /**
       * @brief Set minimum log levels
       * @details Only messages with a level greater than or equal to the minimum output level will be output to standard output or file
       * @param outLevel Minimum standard output level, default is QtMsgType::QtDebugMsg
       * @param fileLevel Minimum file output level, default is QtMsgType::QtDebugMsg
       * @attention Messages with QtDebugMsg level will be processed only if compiled in debug mode, regardless of the minimum log levels
       *            Messages with a QtFatalMsg processed always, regardless of the minimum log levels
       * @attention Minimum standard output level will be ignored if clean log category is set
       * @attention Call this method before creating threads and starting the application event loop
       */
      static void setMinLevels(QtMsgType outLevel, QtMsgType fileLevel) { m_minOutLevel=outLevel; m_minOutFileLevel=fileLevel; }

      /**
       * @brief Set clean log category
       * @details Clean log category is useful for cleaning logs for CI/CD or other automation purposes, e.g. "CI/CD" or "NECESSARY"
       * @details If clean log category is set, then only logs with this category will be output to standard output without any formatting
       * @param category Clean log category name
       * @param writeToFile Write clean log category messages to file and overrided sendLog(), default is true
       * @attention If clean log category is set then minimum standard output level will be ignored
       * @attention Call this method before creating threads and starting the application event loop
       * @attention If automation deals with sensitive data like keys or secrets, it is STRONGLY recommended to set writeToFile to false
       */
      static void setCleanLogCategory(const QString& category, bool writeToFile=true) {
         if(category.isEmpty()) m_cleanLogCategoryIsSet=false; else m_cleanLogCategoryIsSet=true;
         m_cleanLogCategory=category; m_cleanToFile=writeToFile; }

      /**
       * @brief Check if clean log category is set
       * @return Result of the check
       * @retval true Clean log category is set
       * @retval false Clean log category is not set
       * @details This method is fast and thread-safe
       */
      static bool haveCleanCategory() { return m_cleanLogCategoryIsSet; }

      /**
       * @brief Set UTC time mode
       * @details If UTC time mode is set, then all log messages will be written in UTC time
       * @param utcMode UTC time mode
       */
      static void setUtcMode(bool utcMode) { m_utcMode=utcMode; }

      /**
       * @brief Get average buffer flush time
       * @return Average buffer flush time in seconds
       * @details This method is thread-safe
       */
      static float averageBufferFlushTime() { return m_logBufferFlushTime; }

      /**
       * @brief Get average log files rotation time
       * @return Average log rotation time in seconds
       * @details This method is thread-safe
       */
      static float averageRotationTime() { return m_logRotationTime; }

      /**
       * @brief Initialize logging
       * @details Set log files directory and install message handler
       * @param logDir Log files directory, default is empty, which means that logs will be written to the application directory
       * @param flushTime Buffer flush time in milliseconds, default is 10000 ms (10 seconds), less than 1000 ms means buffering is disabled
       * @param maxFiles Maximum number of separate log files, default is 10, minimum is 2 for rotation
       * @param maxFileSize Maximum size of a single log file, default is 10 MB, minimum is 100 KB
       * @return Result of the initialization
       * @retval true Initialization was successful
       * @retval false Initialization failed, e.g. log directory is not writable
       * @details Messages with a critical level or higher cause the buffer to be flushed to a file immediately
       * @attention Call this method before creating threads and starting the application event loop
       * @attention Disabling the buffering is strongly not recommended, as it can cause a disk performance serious drop
       */
      static bool initLogging(QString logDir=QString(), quint32 flushTime=10000, quint32 maxFiles=10, quint32 maxFileSize=(10*1024*1024));

      /**
       * @brief Log message handler
       * @details This method is called by Qt message handler to custom process log messages
       * @param type The type of the message, e.g. debug, info, warning, critical, fatal
       * @param context The context of the message, e.g. category, file, function, line
       * @param msg The message text
       * @attention QtMsgType::QtFatalMsg level is not recommended for use, because it causes the application to automatically terminate with a full core dump
       *            Also in this case the message will be sent to standard and file outputs regardless of the set minimum levels or clean log category
       */
      static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);

   private:
      QCustomLog(const QCustomLog&)=delete; /**< Prohibit copy constructor */
      QCustomLog& operator=(const QCustomLog&)=delete; /**< Prohibit copy assignment */

      static QCustomLog& instance() /**< Singleton with custom inheritor support */
      {
         static QCustomLog defaultInstance;
         return m_customInstance ? *m_customInstance : defaultInstance;
      }

      static void flushBuffer() { QCustomLog::flushBuffer(false); }; /**< Overloaded method for internal purposes */
      static void flushBuffer(bool force=false); /**< Flushes log buffer to file with optional force flush */
      static void callErrorHandler(const QString& msg); /**< Calls error handler with message if it is set */
      static bool ensureDirectoryWritable(const QString& dirPath); /**< Ensures that the directory is writable */
      static void normalizePath(QString& path); /**< Normalizes the path */

      static bool rotateLogFiles(QString& logFileName); /**< Rotates log files within the limits based on the current log file name */
      static bool logFileTouch(const QString& path); /**< Creates an empty log file with the specified path */
      static inline bool levelGreaterOrEqual(QtMsgType level, QtMsgType minLevel); /**< Checks if the level is greater or equal to the minimum level */

      static inline QCustomLog* m_customInstance=nullptr; /**< Custom inheritor storage */
      static inline ErrorHandler m_errorHandler=nullptr; /**< Error handler storage */
      static inline QString m_cleanLogCategory; /**< Clean log category storage */
      static inline std::atomic<bool> m_cleanLogCategoryIsSet=false; /**< Clean log category set flag */
      static inline bool m_cleanToFile=true; /**< Clean log category to file flag */
      static inline QtMsgType m_minOutLevel=QtMsgType::QtDebugMsg; /**< Minimum output level storage */
      static inline QtMsgType m_minOutFileLevel=QtMsgType::QtDebugMsg; /**< Minimum output file level storage */
      static inline QString m_logMessageFormat="'['yyyy.MM.dd HH:mm:ss.zzz']'"; /**< Log message timestamp format */

      static inline QMutex m_logBufferMutex; /**< Mutex for log buffer */
      static inline QMutex m_logFileMutex; /**< Mutex for log file operations */
      static inline QMutex m_customHandlerMutex; /**< Mutex for custom log handler operations */
      static inline QMutex M_errorHandlerMutex; /**< Mutex for error handler operations */

      static inline QDir m_logDir=QDir(); /**< Log files directory */
      static inline QString m_logFileName; /**< Current log file name */

      static inline quint32 m_maxLogFiles=10; /**< Maximum number of log files */
      static inline quint32 m_maxLogFileSize=(10*1024*1024); /**< Maximum size of a log file */

      static inline QTimer m_logBufferTimer=QTimer(nullptr); /**< Buffer flush timer */
      static inline QQueue<QString> m_logBuffer; /**< Log message buffer */
      static inline quint32 m_maxBufferMessages=0; /**< Maximum detected messages in the buffer */
      static inline bool m_logBufferEnabled=false; /**< Buffering state, thread-safe for reading */

      static inline std::atomic<float> m_logBufferFlushTime=0.0f; /**< Average buffer flush time in seconds */
      static inline std::atomic<float> m_logRotationTime=0.0f; /**< Average log rotation time in seconds */

   protected:
      explicit QCustomLog() {} /**< Prohibit direct instantiation */
      virtual ~QCustomLog() { QCustomLog::flushBuffer(false); } /**< Polymorphic destructor */

      virtual void sendLog(const QDateTime& time, const QtMsgType type, const QString& category, const QString& msg) {} /**< Custom log message handler for inheritor */

      static inline bool m_utcMode=false; /**< UTC time flag */
};

#endif // QCUSTOMLOG_H
