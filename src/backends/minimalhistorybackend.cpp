/************************************************************************************
 *   Copyright (C) 2014-2015 by Savoir-Faire Linux                                       *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/
#include "minimalhistorybackend.h"

//Qt
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QHash>
#include <QtWidgets/QApplication>
#include <QtCore/QStandardPaths>

//Ring
#include <call.h>
#include <account.h>
#include <person.h>
#include <contactmethod.h>
#include <historymodel.h>

class MinimalHistoryEditor : public CollectionEditor<Call>
{
public:
   MinimalHistoryEditor(CollectionMediator<Call>* m, MinimalHistoryBackend* parent);
   virtual bool save       ( const Call* item ) override;
   virtual bool remove     ( const Call* item ) override;
   virtual bool edit       ( Call*       item ) override;
   virtual bool addNew     ( const Call* item ) override;
   virtual bool addExisting( const Call* item ) override;

private:
   virtual QVector<Call*> items() const override;

   //Helpers
   void saveCall(QTextStream& stream, const Call* call);
   bool regenFile(const Call* toIgnore);

   //Attributes
   QVector<Call*> m_lItems;
   MinimalHistoryBackend* m_pCollection;
};

MinimalHistoryEditor::MinimalHistoryEditor(CollectionMediator<Call>* m, MinimalHistoryBackend* parent) :
CollectionEditor<Call>(m),m_pCollection(parent)
{

}

MinimalHistoryBackend::MinimalHistoryBackend(CollectionMediator<Call>* mediator) :
CollectionInterface(new MinimalHistoryEditor(mediator,this)),m_pMediator(mediator)
{
//    setObjectName("MinimalHistoryBackend");
}

MinimalHistoryBackend::~MinimalHistoryBackend()
{

}

void MinimalHistoryEditor::saveCall(QTextStream& stream, const Call* call)
{
   const QString direction = (call->direction()==Call::Direction::INCOMING)?
      Call::HistoryStateName::INCOMING : Call::HistoryStateName::OUTGOING;

   const Account* a = call->account();
   stream << QString("%1=%2\n").arg(Call::HistoryMapFields::CALLID          ).arg(call->historyId()                     );
   stream << QString("%1=%2\n").arg(Call::HistoryMapFields::TIMESTAMP_START ).arg(call->startTimeStamp()         );
   stream << QString("%1=%2\n").arg(Call::HistoryMapFields::TIMESTAMP_STOP  ).arg(call->stopTimeStamp()          );
   stream << QString("%1=%2\n").arg(Call::HistoryMapFields::ACCOUNT_ID      ).arg(a?QString(a->id()):""          );
   stream << QString("%1=%2\n").arg(Call::HistoryMapFields::DISPLAY_NAME    ).arg(call->peerName()               );
   stream << QString("%1=%2\n").arg(Call::HistoryMapFields::PEER_NUMBER     ).arg(call->peerContactMethod()->uri() );
   stream << QString("%1=%2\n").arg(Call::HistoryMapFields::DIRECTION       ).arg(direction                      );
   stream << QString("%1=%2\n").arg(Call::HistoryMapFields::MISSED          ).arg(call->isMissed()               );
   stream << QString("%1=%2\n").arg(Call::HistoryMapFields::RECORDING_PATH  ).arg(call->recordingPath()          );
   stream << QString("%1=%2\n").arg(Call::HistoryMapFields::CONTACT_USED    ).arg(false                          );//TODO
   if (call->peerContactMethod()->contact()) {
      stream << QString("%1=%2\n").arg(Call::HistoryMapFields::CONTACT_UID  ).arg(
         QString(call->peerContactMethod()->contact()->uid())
      );
   }
   stream << "\n";
   stream.flush();
}

bool MinimalHistoryEditor::regenFile(const Call* toIgnore)
{
   QDir dir(QString('/'));
   dir.mkpath(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') + QString());

   QFile file(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') +"history.ini");
   if ( file.open(QIODevice::WriteOnly | QIODevice::Text) ) {
      QTextStream stream(&file);
      for (const Call* c : HistoryModel::instance()->getHistoryCalls()) {
         if (c != toIgnore)
            saveCall(stream, c);
      }
      file.close();
      return true;
   }
   return false;
}

bool MinimalHistoryEditor::save(const Call* call)
{
   if (call->collection()->editor<Call>() != this)
      return addNew(call);

   return regenFile(nullptr);
}

bool MinimalHistoryEditor::remove(const Call* item)
{
   return regenFile(item);
}

bool MinimalHistoryEditor::edit( Call* item)
{
   Q_UNUSED(item)
   return false;
}

bool MinimalHistoryEditor::addNew(const Call* call)
{
   QDir dir(QString('/'));
   dir.mkpath(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') + QString());

   if ((call->collection() && call->collection()->editor<Call>() == this)  || call->historyId().isEmpty()) return false;
   //TODO support \r and \n\r end of line
   QFile file(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/')+"history.ini");

   if ( file.open(QIODevice::Append | QIODevice::Text) ) {
      QTextStream streamFileOut(&file);
      saveCall(streamFileOut, call);
      file.close();

      const_cast<Call*>(call)->setCollection(m_pCollection);
      addExisting(call);
      return true;
   }
   else
      qWarning() << "Unable to save history";
   return false;
}

bool MinimalHistoryEditor::addExisting(const Call* item)
{
   m_lItems << const_cast<Call*>(item);
   mediator()->addItem(item);
   return true;
}

QVector<Call*> MinimalHistoryEditor::items() const
{
   return m_lItems;
}

QString MinimalHistoryBackend::name () const
{
   return QObject::tr("Minimal history backend");
}

QString MinimalHistoryBackend::category () const
{
   return QObject::tr("History");
}

QVariant MinimalHistoryBackend::icon() const
{
   return QVariant();
}

bool MinimalHistoryBackend::isEnabled() const
{
   return true;
}

bool MinimalHistoryBackend::load()
{
   QFile file(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') +"history.ini");
   if ( file.open(QIODevice::ReadOnly | QIODevice::Text) ) {
      QMap<QString,QString> hc;
      while (!file.atEnd()) {
         QByteArray line = file.readLine().trimmed();

         //The item is complete
         if ((line.isEmpty() || !line.size()) && hc.size()) {
            Call* pastCall = Call::buildHistoryCall(hc);
            if (pastCall->peerName().isEmpty()) {
               pastCall->setPeerName(QObject::tr("Unknown"));
            }
            pastCall->setRecordingPath(hc[ Call::HistoryMapFields::RECORDING_PATH ]);
            pastCall->setCollection(this);

            editor<Call>()->addExisting(pastCall);
            hc.clear();
         }
         // Add to the current set
         else {
            const int idx = line.indexOf("=");
            if (idx >= 0)
               hc[line.left(idx)] = line.right(line.size()-idx-1);
         }
      }
      return true;
   }
   else
      qWarning() << "History doesn't exist or is not readable";
   return false;
}

bool MinimalHistoryBackend::reload()
{
   return false;
}

CollectionInterface::SupportedFeatures MinimalHistoryBackend::supportedFeatures() const
{
   return (CollectionInterface::SupportedFeatures) (
      CollectionInterface::SupportedFeatures::NONE  |
      CollectionInterface::SupportedFeatures::LOAD  |
      CollectionInterface::SupportedFeatures::CLEAR |
      CollectionInterface::SupportedFeatures::REMOVE|
      CollectionInterface::SupportedFeatures::ADD   );
}

bool MinimalHistoryBackend::clear()
{
   /* TODO: replace with gtk dialog ?  */
   // const int ret = KMessageBox::questionYesNo(static_cast<QApplication*>(QApplication::instance())->activeWindow(), i18n("Are you sure you want to clear history?"), i18n("Clear history"));
   // if (ret == KMessageBox::Yes) {
      QFile::remove(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') + "history.ini");
      return true;
   // }
   // return false;
}

QByteArray MinimalHistoryBackend::id() const
{
   return "mhb";
}
