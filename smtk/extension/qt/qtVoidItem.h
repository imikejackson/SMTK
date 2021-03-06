//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
// .NAME qtVoidItem - a qt item for Void type attribute items
// .SECTION Description
// .SECTION See Also
// qtItem

#ifndef __smtk_extension_qtVoidItem_h
#define __smtk_extension_qtVoidItem_h

#include "smtk/extension/qt/Exports.h"
#include "smtk/extension/qt/qtItem.h"

class qtVoidItemInternals;

namespace smtk
{
namespace extension
{
class SMTKQTEXT_EXPORT qtVoidItem : public qtItem
{
  Q_OBJECT

public:
  qtVoidItem(smtk::attribute::ItemPtr, QWidget* parent, qtBaseView* bview);
  virtual ~qtVoidItem();
  void setLabelVisible(bool) override;

public slots:
  void setOutputOptional(int);

protected slots:
  void updateItemData() override;

protected:
  void createWidget() override;

private:
  qtVoidItemInternals* Internals;

}; // class
}; // namespace attribute
}; // namespace smtk

#endif
