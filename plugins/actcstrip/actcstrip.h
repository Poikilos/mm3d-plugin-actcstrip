#ifndef __ACTCTRIPRIM_H
#define __ACTCTRIPRIM_H

#include "triprim.h"

class ActcTriPrim : public TriPrim
{
   public:
      ActcTriPrim();
      virtual ~ActcTriPrim();

      // grouped:
      //    true  = textures and texture coordinates must match
      //    false = ignore texture information
      //
      // Returns:
      //    true  = found strips, fans
      //    false = error parsing triangle data
      bool findPrimitives( Model * model, bool grouped = true );

      void resetList(); // Reset to start of list
      TriPrim::TriPrimTypeE getNextPrimitive( TriPrim::TriangleVertexList & tvl );

   protected:
      struct Primitive_t
      {
         TriPrimTypeE       primType;
         TriangleVertexList tvl;
      } Primitive_t;
      typedef struct Primitive_t PrimitiveT;

      typedef std::vector< PrimitiveT > PrimitiveList;
      
      PrimitiveList           m_primitives;
      PrimitiveList::iterator m_it;
};

#endif // __ACTCTRIPRIM_H
