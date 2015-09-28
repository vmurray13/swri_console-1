#include <swri_console/log_database_proxy_model.h>
#include <swri_console/log_database.h>
#include <QColor>
#include <QTimer>
#include <stdio.h>
#include <ros/time.h>

namespace swri_console
{
LogDatabaseProxyModel::LogDatabaseProxyModel(LogDatabase *db)
  :
  colorize_logs_(true),
  db_(db),
  debug_color_(Qt::gray),
  info_color_(Qt::black),
  warn_color_(Qt::yellow),
  error_color_(Qt::red),
  fatal_color_(Qt::magenta)
{  
  QObject::connect(db_, SIGNAL(messagesAdded()),
                   this, SLOT(processNewMessages()));

  QObject::connect(db_, SIGNAL(minTimeUpdated()),
                   this, SLOT(minTimeUpdated()));
}

LogDatabaseProxyModel::~LogDatabaseProxyModel()
{
}

void LogDatabaseProxyModel::setNodeFilter(const std::set<std::string> &names)
{
  names_ = names;
  reset();
}

void LogDatabaseProxyModel::setSeverityFilter(uint8_t severity_mask)
{
  severity_mask_ = severity_mask;
  reset();
}

void LogDatabaseProxyModel::setAbsoluteTime(bool absolute)
{
  if (absolute == display_absolute_time_) {
    return;
  }

  display_absolute_time_ = absolute;

  if (display_time_ && msg_mapping_.size()) {
    Q_EMIT dataChanged(index(0),
                       index(msg_mapping_.size()));
  }
}


void LogDatabaseProxyModel::setColorizeLogs(bool colorize_logs)
{
  if (colorize_logs == colorize_logs_) {
    return;
  }

  colorize_logs_ = colorize_logs;
  if (msg_mapping_.size()) {
    Q_EMIT dataChanged(index(0),
                       index(msg_mapping_.size()));
  }
}

void LogDatabaseProxyModel::setDisplayTime(bool display)
{
  if (display == display_time_) {
    return;
  }

  display_time_ = display;

  if (msg_mapping_.size()) {
    Q_EMIT dataChanged(index(0),
                       index(msg_mapping_.size()));
  }
}

void LogDatabaseProxyModel::setUseRegularExpressions(bool useRegexps)
{
  if (useRegexps == use_regular_expressions_) {
    return;
  }

  use_regular_expressions_ = useRegexps;
  reset();
}

void LogDatabaseProxyModel::setIncludeFilters(
  const QStringList &list)
{
  include_strings_ = list;
  reset();
}

void LogDatabaseProxyModel::setExcludeFilters(
  const QStringList &list)
{
  exclude_strings_ = list;
  reset();
}


void LogDatabaseProxyModel::setIncludeRegexpPattern(const QString& pattern)
{
  include_regexp_.setPattern(pattern);
  reset();
}

void LogDatabaseProxyModel::setExcludeRegexpPattern(const QString& pattern)
{
  exclude_regexp_.setPattern(pattern);
  reset();
}

void LogDatabaseProxyModel::setDebugColor(const QColor& debug_color)
{
  debug_color_ = debug_color;
  reset();
}

void LogDatabaseProxyModel::setInfoColor(const QColor& info_color)
{
  info_color_ = info_color;
  reset();
}

void LogDatabaseProxyModel::setWarnColor(const QColor& warn_color)
{
  warn_color_ = warn_color;
  reset();
}

void LogDatabaseProxyModel::setErrorColor(const QColor& error_color)
{
  error_color_ = error_color;
  reset();
}

void LogDatabaseProxyModel::setFatalColor(const QColor& fatal_color)
{
  fatal_color_ = fatal_color;
  reset();
}

int LogDatabaseProxyModel::rowCount(const QModelIndex &parent) const
{
  if (parent.isValid()) {
    return 0;
  }

  return msg_mapping_.size();
}


bool LogDatabaseProxyModel::isIncludeValid() const
{
  if (use_regular_expressions_ && !include_regexp_.isValid()) {
    return false;
  }
  return true;
}

bool LogDatabaseProxyModel::isExcludeValid() const
{
  if (use_regular_expressions_ && !exclude_regexp_.isValid()) {
    return false;
  }
  return true;
}


QVariant LogDatabaseProxyModel::data(
  const QModelIndex &index, int role) const
{
  switch (role)
  {
    // Currently we're only returning data for these roles, so return immediately
    // if we're being queried for anything else.
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
      break;
    case Qt::ForegroundRole:
      if (colorize_logs_) {
        break;
      }
    default:
      return QVariant();
  }

  if (index.parent().isValid() &&
      index.row() >= msg_mapping_.size()) {
    return QVariant();
  }

  const LogEntry &item = db_->log()[msg_mapping_[index.row()]];

  if (role == Qt::DisplayRole) {
    char level = '?';
    if (item.level == rosgraph_msgs::Log::DEBUG) {
      level = 'D';
    } else if (item.level == rosgraph_msgs::Log::INFO) {
      level = 'I';
    } else if (item.level == rosgraph_msgs::Log::WARN) {
      level = 'W';
    } else if (item.level == rosgraph_msgs::Log::ERROR) {
      level = 'E';
    } else if (item.level == rosgraph_msgs::Log::FATAL) {
      level = 'F';
    }

    char stamp[128];
    if (display_absolute_time_) {
      snprintf(stamp, sizeof(stamp),
               "%u.%09u",
               item.stamp.sec,
               item.stamp.nsec);
    } else {
      ros::Duration t = item.stamp - db_->minTime();

      int32_t secs = t.sec;
      int hours = secs / 60 / 60;
      int minutes = (secs / 60) % 60;
      int seconds = (secs % 60);
      int milliseconds = t.nsec / 1000000;
      
      snprintf(stamp, sizeof(stamp),
               "%d:%02d:%02d:%03d",
               hours, minutes, seconds, milliseconds);
    }

    char header[1024];
    if (display_time_) {
      snprintf(header, sizeof(header),
               "[%c %s] ", level, stamp);
    } else {
      snprintf(header, sizeof(header),
               "[%c] ", level);
    }
      
    return QVariant(QString(header) + item.msg);
  }
  else if (role == Qt::ForegroundRole && colorize_logs_) {
    switch (item.level) {
      case rosgraph_msgs::Log::DEBUG:
        return QVariant(debug_color_);
      case rosgraph_msgs::Log::INFO:
        return QVariant(info_color_);
      case rosgraph_msgs::Log::WARN:
        return QVariant(warn_color_);
      case rosgraph_msgs::Log::ERROR:
        return QVariant(error_color_);
      case rosgraph_msgs::Log::FATAL:
        return QVariant(fatal_color_);
      default:
        return QVariant(info_color_);
    }
  }
  else if (role == Qt::ToolTipRole) {
    char buffer[4096];
    snprintf(buffer, sizeof(buffer),
             "<p style='white-space:pre'>"
             "Timestamp: %d.%09d\n"
             "Node: %s\n"
             "Function: %s\n"
             "File: %s\n"
             "Line: %d\n"
             "\n",
             item.stamp.sec,
             item.stamp.nsec,
             item.node.c_str(),
             item.function.c_str(),
             item.file.c_str(),
             item.line);
    
    QString text = (QString(buffer) +
                    item.msg + 
                    QString("</p>"));
                            
    return QVariant(text);
  }
  
  return QVariant();
}

void LogDatabaseProxyModel::clear()
{
  db_->clear();
  reset();
}

void LogDatabaseProxyModel::reset()
{
  beginResetModel();
  msg_mapping_.clear();
  early_mapping_.clear();
  earliest_index_ = db_->log().size();
  latest_index_ = earliest_index_;
  endResetModel();
  scheduleIdleProcessing();
}

void LogDatabaseProxyModel::processNewMessages()
{
  std::deque<size_t> new_items;
 
  // Process all messages from latest_index_ to the end of the
  // log.
  for (; 
       latest_index_ < db_->log().size();
       latest_index_++)
  {
    const LogEntry &item = db_->log()[latest_index_];    
    if (!acceptLogEntry(item)) {
      continue;
    }    
    new_items.push_back(latest_index_);
  }
  
  if (!new_items.empty()) {
    beginInsertRows(QModelIndex(),
                    msg_mapping_.size(),
                    msg_mapping_.size() + new_items.size() - 1);
    msg_mapping_.insert(msg_mapping_.end(),
                        new_items.begin(),
                        new_items.end());
    endInsertRows();

    Q_EMIT messagesAdded();
  }  
}

void LogDatabaseProxyModel::processOldMessages()
{
  //std::deque<size_t> new_items;
  
  for (size_t i = 0;
       earliest_index_ != 0 && i < 100;
       earliest_index_--, i++)
  {
    const LogEntry &item = db_->log()[earliest_index_-1];
    if (!acceptLogEntry(item)) {
      continue;
    }    
    early_mapping_.push_front(earliest_index_-1);
  }
 
  if ((earliest_index_ == 0 && early_mapping_.size()) ||
      (early_mapping_.size() > 200)) {
    beginInsertRows(QModelIndex(),
                    0,
                    early_mapping_.size() - 1);
    msg_mapping_.insert(msg_mapping_.begin(),
                        early_mapping_.begin(),
                        early_mapping_.end());
    early_mapping_.clear();
    endInsertRows();

    Q_EMIT messagesAdded();
  }

  scheduleIdleProcessing();
}

void LogDatabaseProxyModel::scheduleIdleProcessing()
{
  // If we have older logs that still need to be processed, schedule a
  // callback at the next idle time.
  if (earliest_index_ > 0) {
    QTimer::singleShot(0, this, SLOT(processOldMessages()));
  }
}

bool LogDatabaseProxyModel::acceptLogEntry(const LogEntry &item)
{
  if (!(item.level & severity_mask_)) {
    return false;
  }
  
  if (names_.count(item.node) == 0) {
    return false;
  }

  if (!testIncludeFilter(item)) {
    return false;
  }

  if (use_regular_expressions_) {
    // Don't let an empty regexp filter out everything
    return exclude_regexp_.isEmpty() || exclude_regexp_.indexIn(item.msg) < 0;
  } else {
    for (int i = 0; i < exclude_strings_.size(); i++) {
      if (item.msg.contains(exclude_strings_[i], Qt::CaseInsensitive)) {
        return false;
      }
    }
  }
  
  return true;
}

// Return true if the item message contains at least one of the
// strings in include_filter_.  Always returns true if there are no
// include strings.
bool LogDatabaseProxyModel::testIncludeFilter(const LogEntry &item)
{
  if (use_regular_expressions_) {
    return include_regexp_.indexIn(item.msg) >= 0;
  } else {
    if (include_strings_.empty()) {
      return true;
    }

    for (int i = 0; i < include_strings_.size(); i++) {
      if (item.msg.contains(include_strings_[i], Qt::CaseInsensitive)) {
        return true;
      }
    }
  }

  return false;
}

void LogDatabaseProxyModel::minTimeUpdated()
{
  if (display_time_ &&
      !display_absolute_time_
      && msg_mapping_.size()) {
    Q_EMIT dataChanged(index(0), index(msg_mapping_.size()));
  }  
}


}  // namespace swri_console
