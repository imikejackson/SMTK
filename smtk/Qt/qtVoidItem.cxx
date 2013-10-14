/*=========================================================================

Copyright (c) 1998-2003 Kitware Inc. 469 Clifton Corporate Parkway,
Clifton Park, NY, 12065, USA.

All rights reserved. No part of this software may be reproduced, distributed,
or modified, in any form or by any means, without permission in writing from
Kitware Inc.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN
"AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

=========================================================================*/

#include "smtk/Qt/qtVoidItem.h"

#include <QCheckBox>
#include <QSizePolicy>

#include "smtk/attribute/VoidItem.h"
#include "smtk/attribute/VoidItemDefinition.h"

using namespace smtk::attribute;

//----------------------------------------------------------------------------
class qtVoidItemInternals
{
public:

};


//----------------------------------------------------------------------------
qtVoidItem::qtVoidItem(
  smtk::AttributeItemPtr dataObj, QWidget* p) : qtItem(dataObj, p)
{
  this->Internals = new qtVoidItemInternals;
  this->IsLeafItem = true;
  this->createWidget();
}

//----------------------------------------------------------------------------
qtVoidItem::~qtVoidItem()
{
  delete this->Internals;
}
//----------------------------------------------------------------------------
void qtVoidItem::createWidget()
{
  smtk::AttributeItemPtr dataObj = this->getObject();
  if(!dataObj || !this->passAdvancedCheck() || !dataObj->isOptional())
    {
    return;
    }

  this->clearChildItems();

  QCheckBox* optionalCheck = new QCheckBox(this->parentWidget());
  optionalCheck->setChecked(dataObj->definition()->isEnabledByDefault());
  QSizePolicy sizeFixedPolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  optionalCheck->setSizePolicy(sizeFixedPolicy);
  QString txtLabel = dataObj->definition()->label().empty() ?
    dataObj->definition()->name().c_str() : dataObj->definition()->label().c_str();
  optionalCheck->setText(txtLabel);
  QObject::connect(optionalCheck, SIGNAL(stateChanged(int)),
    this, SLOT(setOutputOptional(int)));

  this->Widget = optionalCheck;

  this->updateItemData();
}

//----------------------------------------------------------------------------
void qtVoidItem::updateItemData()
{
  smtk::AttributeItemPtr dataObj = this->getObject();
  if(!dataObj || !dataObj->isOptional() || !this->Widget)
    {
    return;
    }
  QCheckBox* optionalCheck = qobject_cast<QCheckBox*>(this->Widget);
  optionalCheck->setChecked(dataObj->isEnabled());
}
//----------------------------------------------------------------------------
void qtVoidItem::setOutputOptional(int state)
{
  this->getObject()->setIsEnabled(state ? true : false);
}