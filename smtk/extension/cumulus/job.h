//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
// .NAME job.h
// .SECTION Description
// .SECTION See Also

#ifndef __smtk_extension_cumulus_job_h
#define __smtk_extension_cumulus_job_h

#include "smtk/extension/cumulus/Exports.h"

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>

struct cJSON;

namespace cumulus
{

class SMTKCUMULUSEXT_EXPORT Job
{
public:
  Job();
  Job(const QString& id, const QString& name, const QString& status,
    const QList<QString>& outputFolderIds, const QString& machine);
  Job(const Job& job);

  ~Job();
  QString id() const { return this->m_id; };
  QString name() const { return this->m_name; };
  QString status() const { return this->m_status; };
  QString machine() const { return this->m_machine; };
  QList<QString> outputFolderIds() const { return this->m_outputFolderIds; };
  QString notes() const { return this->m_notes; };
  int numberOfNodes() const { return this->m_numberOfNodes; }
  int numberOfCores() const { return this->m_numberOfCores; }
  QDateTime started() const { return this->m_start; };
  QDateTime finished() const { return this->m_finish; };
  QString downloadFolder() const { return this->m_downloadFolder; }

  bool isValid() const { return !this->m_id.isEmpty(); };
  void setStatus(const QString& status);
  void setDownloadFolder(const QString& path);

  static Job fromJSON(cJSON* obj);

  // Copy cmb-specific data to json (for patch request)
  cJSON* cmbDataToJSON() const;

private:
  QString m_id;
  QString m_name;
  QString m_status;
  QString m_machine;
  QList<QString> m_outputFolderIds;
  QString m_notes;
  int m_numberOfNodes;
  int m_numberOfCores;
  QDateTime m_start;
  QDateTime m_finish;
  QString m_downloadFolder;
};

}; // end namespace

Q_DECLARE_METATYPE(cumulus::Job)

#endif
