#include "actcstrip.h"

#include "model.h"
#include "log.h"
#include "pluginapi.h"
#include "version.h"

#include <list>

#include <math.h>
#include <stdlib.h>
#include <ctype.h>

extern "C" {
#include "actc/tc.h"
};

enum EdgeState_e
{
   ES_MISMATCH = -1,
   ES_NOMATCH  = 0,
   ES_MATCH    = 1
};
typedef enum EdgeState_e EdgeStateE;

struct TriElement_t
{
   unsigned triId;
   int textureId;

   unsigned v[3]; // vertex indices

   float s[3]; // texture coords
   float t[3];

   bool used;
   EdgeStateE es;
};
typedef struct TriElement_t TriElementT;

typedef std::vector< TriElementT > TriElementList;

static int _haveUnused( TriElementList & l )
{
   unsigned count = l.size();

   for( unsigned t = 0; t < count; t++ )
   {
      if ( !l[t].used )
      {
         return t;
      }
   }
   return -1;
}

static EdgeStateE _edgeMatch( TriElementList & l, TriElementT & te )
{
   unsigned count = l.size();
   for ( unsigned t = 0; t < count; t++ )
   {
      unsigned vertMatch = 0;

      for ( unsigned i = 0; i < 3; i++ )
      {
         unsigned v1 = te.v[i];
         for ( unsigned j = 0; j < 3; j++ )
         {
            unsigned v2 = l[t].v[j];

            if ( v1 == v2 )
            {
               if ( te.s[i] == l[t].s[j] && te.t[i] == l[t].t[j] )
               {
                  vertMatch++;
               }
               else
               {
                  // Found verts that don't share coordinates
                  // We're done here
                  return ES_MISMATCH;
               }
            }
         }
      }

      if ( vertMatch >= 2 )
      {
         return ES_MATCH;
      }
   }

   return ES_NOMATCH;
}

static void _getVertexTexCoords( TriElementList & l, unsigned v, float & s, float & t )
{
   unsigned count = l.size();
   for ( unsigned n = 0; n < count; n++ )
   {
      for ( unsigned i = 0; i < 3; i++ )
      {
         if ( l[n].v[i] == v )
         {
            s = l[n].s[i];
            t = l[n].t[i];
            return;
         }
      }
   }

   s = 0.0f;
   t = 0.0f;
}

static int _getMappedVertex( std::vector<int> & vectormap, int v )
{
   unsigned count = vectormap.size();
   unsigned t = 0;
   for ( t = 0; t < count; t++ )
   {
      if ( vectormap[t] == v )
      {
         return t;
      }
   }
   vectormap.push_back( v );

   return count;
}

ActcTriPrim::ActcTriPrim()
   : m_it( m_primitives.end() )
{
}

ActcTriPrim::~ActcTriPrim()
{
}

bool ActcTriPrim::findPrimitives( Model * model, bool grouped )
{
   bool error = false;

   m_primitives.clear();

   unsigned count = model->getTriangleCount();
   unsigned t = 0;

   // 1. Put all untextured triangles in one group
   //
   // 2. Find primitives for this group
   // 
   // 3. Start a new triangle group
   //
   // 4. Add one new triangle to group
   //
   // 5. Add adjacent triangles with matching texture coords recursively
   //
   // 6. Find primitives for this group
   //
   // 7. Repeat from step 3 until done

   ACTCData * tc = actcNew();

   if ( grouped )
   {
      // Set up triangle groups
      TriElementList l;
      TriElementT te;
      std::vector<int> vectormap;
      
      te.textureId = -1;
      te.used      = false;
      for ( unsigned i = 0; i < 3; i++ )
      {
         te.textureId = -1;
         te.v[i] = 0;
         te.s[i] = 0.0f;
         te.t[i] = 0.0f;
      }

      unsigned tricount = model->getTriangleCount();
      l.resize( tricount, te );

      unsigned groupCount = model->getGroupCount();
      for ( unsigned g = 0; g < groupCount; g++ )
      {
         std::list<int> trilist = model->getGroupTriangles( g );

         std::list<int>::iterator it;

         for ( it = trilist.begin(); it != trilist.end(); it++ )
         {
            unsigned tri = *it;
            l[tri].textureId = model->getGroupTextureId( g );
            l[tri].used = true;
            for ( unsigned n = 0; n < 3; n++ )
            {
               l[tri].v[n] = model->getTriangleVertex( tri, n );
               model->getTextureCoords( tri, n, l[tri].s[n], l[tri].t[n] );
            }
         }
      }

      unsigned t = 0;

      for ( t = 0; t < tricount; t++ )
      {
         l[t].triId = t;
         l[t].es   = ES_NOMATCH;

         if ( ! l[t].used )
         {
            for ( unsigned n = 0; n < 3; n++ )
            {
               l[t].v[n] = model->getTriangleVertex( t, n );
            }
         }
         l[t].used = false;
      }

      // Mark triangles that have two or more identical vertices as "used"

      for ( t = 0; t < tricount; t++ )
      {
         if ( l[t].v[0] == l[t].v[1]
           || l[t].v[0] == l[t].v[2]
           || l[t].v[1] == l[t].v[2] )
         {
            log_warning( "model has invisible triangle (two or more identical vertices)\n" );
            l[t].used = true;
         }
      }

      // Find primitives for ungrouped
      {
         log_debug( "finding ungrouped triangles\n" );
         vectormap.clear();

         actcBeginInput( tc );
         for ( t = 0; t < tricount; t++ )
         {
            if ( l[t].textureId == -1 )
            {
               l[t].used = true;
               uint v1 = _getMappedVertex( vectormap, model->getTriangleVertex( t, 0 ) );
               uint v2 = _getMappedVertex( vectormap, model->getTriangleVertex( t, 1 ) );
               uint v3 = _getMappedVertex( vectormap, model->getTriangleVertex( t, 2 ) );
               /*
               uint v1 = model->getTriangleVertex( t, 0 );
               uint v2 = model->getTriangleVertex( t, 1 );
               uint v3 = model->getTriangleVertex( t, 2 );
               */

               log_debug( "vertices %d %d %d\n", v1, v2, v3 );
               actcAddTriangle( tc, v1, v2, v3 );
            }
         }
         actcEndInput( tc );
         log_debug( "done finding ungrouped triangles\n" );

         if ( vectormap.size() )
         {
            log_debug( "finding primitives for ungrouped triangles\n" );
            actcBeginOutput( tc );
            {
               uint v1 = 0;
               uint v2 = 0;
               uint v3 = 0;
               int type = 0;
               TriangleVertexT tv;
               tv.s = 0.0f;
               tv.t = 0.0f;

               while ( !error && (type = actcStartNextPrim( tc, &v1, &v2 )) != ACTC_DATABASE_EMPTY )
               {
                  TriPrimTypeE tpt = TRI_PRIM_NONE;

                  if ( type == ACTC_PRIM_FAN )
                  {
                     log_debug( "got triangle fan\n" );
                     tpt = TRI_PRIM_FAN;
                  }
                  else if ( type == ACTC_PRIM_STRIP )
                  {
                     log_debug( "got triangle strip\n" );
                     tpt = TRI_PRIM_STRIP;
                  }
                  else
                  {
                     error = true;
                  }

                  if ( tpt != TRI_PRIM_NONE )
                  {
                     PrimitiveT prim;
                     prim.primType = tpt;

                     tv.vert = vectormap[ v1 ];
                     //tv.vert = v1;
                     prim.tvl.push_back( tv );

                     log_debug( "out vertex %d = %d\n", v1, vectormap[ v1 ] );

                     tv.vert = vectormap[ v2 ];
                     //tv.vert = v2;
                     prim.tvl.push_back( tv );

                     log_debug( "out vertex %d = %d\n", v2, vectormap[ v2 ] );

                     while ( actcGetNextVert( tc, &v3 ) != ACTC_PRIM_COMPLETE )
                     {
                        tv.vert = vectormap[ v3 ];
                        //tv.vert = v3;
                        log_debug( "out vertex %d = %d\n", v3, vectormap[ v3 ] );
                        prim.tvl.push_back( tv );
                     }

                     m_primitives.push_back( prim );
                  }
               }
            }
            actcEndOutput( tc );
            log_debug( "done finding primitives for ungrouped triangles\n" );
         }
      }

      // Find primitives for grouped
      int first = 0;
      log_debug( "finding grouped triangles with shared vertex coordinates\n" );
      while ( (first = _haveUnused( l )) != -1 )
      {

         vectormap.clear();
         l[first].used = true;

         log_debug( "group starting at %d\n", first );
         TriElementList grp;
         grp.push_back( l[first] );

         for ( t = 0; t < tricount; t++ )
         {
            if ( ! l[t].used )
            {
               l[t].es = ES_NOMATCH;
            }
         }

         bool found = true;
         while ( found )
         {
            found = false;
            for ( t = first + 1; t < tricount; t++ )
            {
               if ( !l[t].used && l[t].es == ES_NOMATCH )
               {
                  if ( (l[t].es = _edgeMatch( grp, l[t] )) == ES_MATCH )
                  {
                     l[t].used = true;
                     grp.push_back( l[t] );
                     found = true;
                  }
               }
            }
         }
         log_debug( "done finding grouped triangles with shared vertex coordinates\n" );

         // Found a group, get primitives
         log_debug( "group size is %d\n", grp.size() );
         if ( grp.size() > 1 )
         {
            log_debug( "finding primitives for grouped triangles with shared vertex coordinates\n" );
            actcMakeEmpty( tc );
            actcBeginInput( tc );
            unsigned grpcount = grp.size();
            for ( unsigned g = 0; g < grpcount; g++ )
            {
               unsigned t = grp[g].triId;
               uint v1 = _getMappedVertex( vectormap, model->getTriangleVertex( t, 0 ) );
               uint v2 = _getMappedVertex( vectormap, model->getTriangleVertex( t, 1 ) );
               uint v3 = _getMappedVertex( vectormap, model->getTriangleVertex( t, 2 ) );
               /*
               uint v1 = model->getTriangleVertex( t, 0 );
               uint v2 = model->getTriangleVertex( t, 1 );
               uint v3 = model->getTriangleVertex( t, 2 );
               */

               actcAddTriangle( tc, v1, v2, v3 );
            }
            actcEndInput( tc );

            actcBeginOutput( tc );
            {
               uint v1 = 0;
               uint v2 = 0;
               uint v3 = 0;
               int type = 0;
               TriangleVertexT tv;
               tv.s = 0.0f;
               tv.t = 0.0f;

               while ( !error && (type = actcStartNextPrim( tc, &v1, &v2 )) != ACTC_DATABASE_EMPTY )
               {
                  TriPrimTypeE tpt = TRI_PRIM_NONE;

                  if ( type == ACTC_PRIM_FAN )
                  {
                     log_debug( "got a triangle fan\n" );
                     tpt = TRI_PRIM_FAN;
                  }
                  else if ( type == ACTC_PRIM_STRIP )
                  {
                     log_debug( "got a triangle strip\n" );
                     tpt = TRI_PRIM_STRIP;
                  }
                  else
                  {
                     error = true;
                  }

                  if ( tpt != TRI_PRIM_NONE )
                  {
                     PrimitiveT prim;
                     prim.primType = tpt;

                     tv.vert = vectormap[ v1 ];
                     //tv.vert = v1;
                     _getVertexTexCoords( grp, tv.vert, tv.s, tv.t );
                     prim.tvl.push_back( tv );

                     log_debug( "out vertex %d = %d\n", v1, vectormap[ v1 ] );

                     tv.vert = vectormap[ v2 ];
                     //tv.vert = v2;
                     _getVertexTexCoords( grp, tv.vert, tv.s, tv.t );
                     prim.tvl.push_back( tv );

                     log_debug( "out vertex %d = %d\n", v2, vectormap[ v2 ] );

                     while ( actcGetNextVert( tc, &v3 ) != ACTC_PRIM_COMPLETE )
                     {
                        tv.vert = vectormap[ v3 ];
                        //tv.vert = v3;

                        _getVertexTexCoords( grp, tv.vert, tv.s, tv.t );
                        prim.tvl.push_back( tv );

                        log_debug( "out vertex %d = %d\n", v3, vectormap[ v3 ] );
                     }

                     m_primitives.push_back( prim );
                  }
               }
            }
            actcEndOutput( tc );
            log_debug( "done finding primitives for grouped triangles with shared vertex coordinates\n" );
         }
         else
         {
            log_debug( "got lone triangle for triangle fan\n" );
            PrimitiveT prim;
            prim.primType = TRI_PRIM_FAN;
            TriangleVertexT tv;

            tv.vert = l[first].v[0];
            tv.s    = l[first].s[0];
            tv.t    = l[first].t[0];
            prim.tvl.push_back( tv );

            tv.vert = l[first].v[1];
            tv.s    = l[first].s[1];
            tv.t    = l[first].t[1];
            prim.tvl.push_back( tv );

            tv.vert = l[first].v[2];
            tv.s    = l[first].s[2];
            tv.t    = l[first].t[2];
            prim.tvl.push_back( tv );

            m_primitives.push_back( prim );
         }
      }
   }
   else
   {
      log_debug( "finding primitives, grouping ignored\n" );
      actcBeginInput( tc );
      for ( t = 0; t < count; t++ )
      {
         uint v1 = model->getTriangleVertex( t, 0 );
         uint v2 = model->getTriangleVertex( t, 1 );
         uint v3 = model->getTriangleVertex( t, 2 );

         actcAddTriangle( tc, v1, v2, v3 );
      }
      actcEndInput( tc );

      actcBeginOutput( tc );
      {
         uint v1 = 0;
         uint v2 = 0;
         uint v3 = 0;
         int type = 0;
         TriangleVertexT tv;
         tv.s = 0.0f;
         tv.t = 0.0f;

         while ( !error && (type = actcStartNextPrim( tc, &v1, &v2 )) != ACTC_DATABASE_EMPTY )
         {
            TriPrimTypeE tpt = TRI_PRIM_NONE;

            if ( type == ACTC_PRIM_FAN )
            {
               tpt = TRI_PRIM_FAN;
            }
            else if ( type == ACTC_PRIM_STRIP )
            {
               tpt = TRI_PRIM_STRIP;
            }
            else
            {
               error = true;
            }

            if ( tpt != TRI_PRIM_NONE )
            {
               PrimitiveT prim;
               prim.primType = tpt;

               tv.vert = v1;
               prim.tvl.push_back( tv );

               tv.vert = v2;
               prim.tvl.push_back( tv );

               while ( actcGetNextVert( tc, &v3 ) != ACTC_PRIM_COMPLETE )
               {
                  tv.vert = v3;
                  prim.tvl.push_back( tv );
               }

               m_primitives.push_back( prim );
            }
         }
      }
      actcEndOutput( tc );

      log_debug( "done finding primitives\n" );
   }

   actcDelete( tc );

   if ( error )
   {
      m_primitives.clear();
   }

   resetList();
   return !error;
}

void ActcTriPrim::resetList()
{
   m_it = m_primitives.begin();
}

TriPrim::TriPrimTypeE ActcTriPrim::getNextPrimitive( TriPrim::TriangleVertexList & tvl )
{
   tvl.clear();

   if ( m_it != m_primitives.end() )
   {
      TriPrimTypeE pt = (*m_it).primType;
      tvl = (*m_it).tvl;
      m_it++;

      return pt;
   }

   return TRI_PRIM_NONE;
}

static TriPrim * _newActcFunc()
{
   return new ActcTriPrim;
}

#ifdef PLUGIN

//------------------------------------------------------------------
// Plugin functions
//------------------------------------------------------------------

PLUGIN_API bool plugin_init()
{
   log_debug( "ACTC Triangle Stripping plugin initialized\n" );
   TriPrim::registerTriPrimFunction( _newActcFunc );
   return true;
}

// We have no cleanup to do
PLUGIN_API bool plugin_uninit()
{
   return true;
}

PLUGIN_API const char * plugin_version()
{
   return "1.2.0";
}

PLUGIN_API const char * plugin_mm3d_version()
{
   return VERSION_STRING;
}

PLUGIN_API const char * plugin_desc()
{
   return "ACTC triangle stripping";
}

#endif // PLUGIN
