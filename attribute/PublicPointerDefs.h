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
// .NAME PublicPointerDefs.h -
// .SECTION Description
// .SECTION See Also

#ifndef __slctk_attribute_PublicPointerDefs_h
#define __slctk_attribute_PublicPointerDefs_h

#include <tr1/memory>

namespace slctk
{
  template <typename T> struct sharedPtr : public std::tr1::shared_ptr<T> {};
  template <typename T> struct weakPtr : public std::tr1::weak_ptr<T>{};
  template <typename T, typename U>
  inline std::tr1::shared_ptr<T> dynamicCastPointer(const std::tr1::shared_ptr<U> &r)
  {
    return std::tr1::dynamic_pointer_cast<T>(r);
  }

  namespace attribute
  {
    class Attribute;
    class AttributeReferenceComponent;
    class AttributeReferenceComponentDefinition;
    class Definition;
    class Cluster;
    class Component;
    class ComponentDefinition;
    class DoubleComponent;
    class DoubleComponentDefinition;
    class GroupComponent;
    class GroupComponentDefinition;
    class IntegerComponent;
    class IntegerComponentDefinition;
    class StringComponent;
    class StringComponentDefinition;
    class ValueComponent;
    class ValueComponentDefinition;
  };

  typedef std::tr1::shared_ptr<attribute::Attribute> AttributePtr;
  typedef std::tr1::weak_ptr<attribute::Attribute> WeakAttributePtr;
  typedef std::tr1::shared_ptr<attribute::Definition> AttributeDefinitionPtr;
  typedef std::tr1::shared_ptr<const attribute::Definition> ConstAttributeDefinitionPtr;
  typedef std::tr1::weak_ptr<attribute::Definition> WeakAttributeDefinitionPtr;

  typedef std::tr1::shared_ptr<attribute::AttributeReferenceComponent> AttributeReferenceComponentPtr;
  typedef std::tr1::shared_ptr<attribute::AttributeReferenceComponentDefinition> AttributeReferenceComponentDefinitionPtr;

  typedef std::tr1::shared_ptr<attribute::Cluster> AttributeClusterPtr;
  typedef std::tr1::weak_ptr<attribute::Cluster> WeakAttributeClusterPtr;

  typedef std::tr1::shared_ptr<attribute::Component> AttributeComponentPtr;
  typedef std::tr1::shared_ptr<const attribute::Component> ConstAttributeComponentPtr;
  typedef std::tr1::shared_ptr<attribute::ComponentDefinition> AttributeComponentDefinitionPtr;
  typedef std::tr1::shared_ptr<const attribute::ComponentDefinition> ConstAttributeComponentDefinitionPtr;
  typedef std::tr1::shared_ptr<attribute::ValueComponent> ValueComponentPtr;
  typedef std::tr1::shared_ptr<attribute::DoubleComponent> DoubleComponentPtr;
  typedef std::tr1::shared_ptr<attribute::DoubleComponentDefinition> DoubleComponentDefinitionPtr;
  typedef std::tr1::shared_ptr<attribute::IntegerComponent> IntegerComponentPtr;
  typedef std::tr1::shared_ptr<attribute::IntegerComponentDefinition> IntegerComponentDefinitionPtr;
  typedef std::tr1::shared_ptr<attribute::StringComponent> StringComponentPtr;
  typedef std::tr1::shared_ptr<attribute::StringComponentDefinition> StringComponentDefinitionPtr;

/*
  template<typename T>
  struct shared_ptr_type
  {
    typedef std::tr1::shared_ptr<T> type;
  };
*/

  template <typename T>
  struct is_shared_ptr
  {
    enum {type=false};
  };
  template<typename T>
  struct is_shared_ptr<std::tr1::shared_ptr<T> >
  {
    enum{type=true};
  };

  template<typename T, int Enabled = is_shared_ptr<T>::type >
  struct shared_ptr_type
  {
    typedef std::tr1::shared_ptr<T> type;
    typedef T T_Type;
  };

template<typename T>
 struct shared_ptr_type<T,true>
  {
    typedef T type;
    typedef typename T::element_type T_Type;
  };

};
#endif /* __slctk_attribute_PublicPointerDefs_h */
