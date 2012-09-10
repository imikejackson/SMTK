/*=========================================================================

Copyright (c) 1998-2012 Kitware Inc. 28 Corporate Drive,
Clifton Park, NY, 12065, USA.

All rights reserved. No part of this software may be reproduced, distributed,
or modified, in any form or by any means, without permission in writing from
Kitware Inc.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN
"AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO
PROVIDE
MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
=========================================================================*/
// .NAME FileItemDefinition.h -
// .SECTION Description
// .SECTION See Also

#ifndef __slctk_attribute_FileItemDefinition_h
#define __slctk_attribute_FileItemDefinition_h

#include "AttributeExports.h"
#include "attribute/PublicPointerDefs.h"

#include "ItemDefinition.h"

namespace slctk
{
  namespace attribute
  {
    class Attribute;
    class Definition;
    class SLCTKATTRIBUTE_EXPORT FileItemDefinition:
      public ItemDefinition
    {
    public:
      FileItemDefinition(const std::string &myName);
      virtual ~FileItemDefinition();
      
      virtual Item::Type type() const;
      bool isValueValid(const std::string &val) const;

      virtual slctk::AttributeItemPtr buildItem() const;
      int numberOfValues() const
      {return this->m_numberOfValues;}
      void setNumberOfValues(int esize);

      bool hasValueLabels() const
      {return this->m_valueLabels.size();}

      void setValueLabel(int element, const std::string &elabel);
      void setCommonValueLabel(const std::string &elabel);
      std::string valueLabel(int element) const;
      bool shouldExist() const
      {return this->m_shouldExist;}
      void setShouldExist(bool val)
      { this->m_shouldExist = val;}
      bool shouldBeRelative() const
      {return this->m_shouldBeRelative;}
      void setShouldBeRelative(bool val)
      {this->m_shouldBeRelative = val;}

    protected:
        bool m_shouldExist;
        bool m_shouldBeRelative;
        bool m_useCommonLabel;
        std::vector<std::string> m_valueLabels;
        int m_numberOfValues;
     private:
      
    };
  };
};

#endif /* __slctk_attribute_FileItemDefinition_h */