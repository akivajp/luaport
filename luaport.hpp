#ifndef _LUAPORT_HPP
#define _LUAPORT_HPP

/////////////////////////////////////////////////////////////////////////////
// Name:        luaport.hpp
// Purpose:     C++ <-> lua porting (binding) library
// Author:      Akiva Miura <akiva.miura@gmail.com>
// Modified by:
// Created:     03/18/2013
// Copyright:   (C) 2013 Akiva Miura
// Licence:     MIT License
/////////////////////////////////////////////////////////////////////////////

#include <lua.hpp>
//#include <lua5.1/lua.hpp>
#include <string>
#include <typeinfo>

namespace luaport
{

  const bool adopt = true;

  void open(lua_State *L);

  template <typename T>
    class object get_class(lua_State *L);
  static class object globals(lua_State *L);
  static class object registry(lua_State *L);

  class object
  {
    public:

      class from_stack
      {
        public:
          from_stack(lua_State *L, int pos)
            : L(L), pos(pos)
          { }

        private:
          lua_State *L;
          int pos;
          friend class object;
      };

      class proxy
      {
        public:
          proxy(lua_State *L, int ref_table, int ref_key)
            : L(L), ref_table(ref_table), ref_key(ref_key)
          {
//printf("REGIST (PROXY TABLE): %d\n", ref_table);
//printf("REGIST (PROXY KEY): %d\n", ref_key);
          }

          ~proxy()
          {
            if (L)
            {
//printf("UNREF (PROXY KEY): %d\n", ref_key);
              luaL_unref(L, LUA_REGISTRYINDEX, ref_key);
//printf("UNREF (PROXY TABLE): %d\n", ref_table);
              luaL_unref(L, LUA_REGISTRYINDEX, ref_table);
            }
          }

          bool is_table() const
          {
            return object(*this).is_table();
          }

          bool is_valid() const
          {
            return object(*this).is_valid();
          }

          object obj()
          {
            return object(*this);
          }

          int type() const
          {
            return object(*this).type();
          }

          template <typename T>
            void operator=(const T &val)
          {
            if (ref_table == LUA_REFNIL)
            {
              luaL_error(L, "[%s:%d] attempt to index a nil value", __FILE__, __LINE__);
              return;
            }
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_table);
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_key);
            object::push(L, val);
            lua_settable(L, -3);
            lua_pop(L, 1);
          }

          template <typename T>
            proxy operator[](const T &key)
          {
            if (ref_table == LUA_REFNIL)
            {
              luaL_error(L, "[%s:%d] attempt to index a nil value", __FILE__, __LINE__);
            }
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_table);
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_key);
            lua_gettable(L, -2);
            int newtable = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF (TABLE): %d\n", newtable);
            lua_pop(L, 1);
            object::push(L, key);
            int newkey = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF (KEY): %d\n", newkey);
            return proxy(L, newtable, newkey);
          }

          operator bool() const
          {
            return object(*this);
          }

        private:
          lua_State *L;
          int ref_table;
          int ref_key;
          friend class object;
      };

      class iterator
      {
        public:
          iterator(const object &table)
            : ref_table(LUA_REFNIL), ref_key(LUA_REFNIL), ref_val(LUA_REFNIL)
          {
            L = table.interpreter();
            if (L)
            {
              table.push();
              ref_table = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF TBL: %d\n", ref_table);
              table.push();
//printf("TBL: %s\n", luaL_tolstring(L, -1, NULL)); lua_pop(L, 1);
//printf("REF PREV: %d\n", ref_key);
              lua_pushnil(L);
              int result = lua_next(L, -2);
//printf("RESULT: %d\n", result);
              if (result == 0)
              {
                clear();
                lua_pop(L, 1);
                return;
              }
              ref_val = luaL_ref(L, LUA_REGISTRYINDEX);
              ref_key = luaL_ref(L, LUA_REGISTRYINDEX);
              lua_pop(L, 1);
            }
          }

          ~iterator()
          {
            clear();
          }

          bool clear()
          {
            if (! L) { return false; }
            if (ref_key == LUA_REFNIL) { return false; }
//printf("UNREF (KEY): %d\n", ref_key);
            luaL_unref(L, LUA_REGISTRYINDEX, ref_key);
            ref_key = LUA_REFNIL;
//printf("UNREF (VAL): %d\n", ref_val);
            luaL_unref(L, LUA_REGISTRYINDEX, ref_val);
            ref_val = LUA_REFNIL;
//printf("UNREF (TABLE): %d\n", ref_table);
            luaL_unref(L, LUA_REGISTRYINDEX, ref_table);
            ref_table = LUA_REFNIL;
            return true;
          }

          bool is_valid() const
          {
            if (ref_key == LUA_REFNIL) { return false; }
            return true;
          }

          object key() const
          {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_key);
            object key(from_stack(L, -1));
            lua_pop(L, 1);
            return key;
          }

          object value() const
          {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_val);
            object val(from_stack(L, -1));
            lua_pop(L, 1);
            return val;
          }

          operator bool() const
          {
            return is_valid();
          }

          proxy operator*() const
          {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_table);
//printf("OLD REF TABLE: %d\n", ref_table);
            int t = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("NEW REF TABLE: %d\n", t);
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_key);
//printf("OLD REF KEY: %d\n", ref_key);
            int k = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("NEW REF KEY: %d\n", k);
            return proxy(L, t, k);
          }

          iterator &operator++()
          {
//printf("TOP: %d\n", lua_gettop(L));
//printf("REF TBL: %d\n", ref_table);
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_table);
//printf("TBL: %s\n", luaL_tolstring(L, -1, NULL)); lua_pop(L, 1);
//printf("REF PREV: %d\n", ref_key);
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_key);
//printf("PREV: %s\n", object(from_stack(L, -1)).tostring().c_str());
//printf("TOP: %d\n", lua_gettop(L));
            int result = lua_next(L, -2);
//printf("TOP: %d\n", lua_gettop(L));
//printf("RESULT: %d\n", result);
            if (result == 0)
            {
              this->clear();
              lua_pop(L, 1);
//printf("TOP: %d\n", lua_gettop(L));
              return *this;
            }
//printf("VAL0: %s\n", object(from_stack(L, -1)).c_str());
            ref_val = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("TOP: %d\n", lua_gettop(L));
//printf("KEY0: %s\n", object(from_stack(L, -1)).c_str());
            ref_key = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF KEY0: %d\n", ref_key);
//printf("TOP: %d\n", lua_gettop(L));
            lua_pop(L, 1);
//printf("TOP: %d\n", lua_gettop(L));
            return *this;
          }

        private:
          lua_State *L;
          int ref_table;
          int ref_key;
          int ref_val;
      };


    public:

      object(lua_State *L)
        : L(L), ref(LUA_REFNIL)
      {
      }

      object()
        : L(NULL), ref(LUA_REFNIL)
      {
      }

      // copy ctor
      object(const object &src)
      {
//printf("COPY!\n");
        L = src.L;
        ref = LUA_REFNIL;
        if (! L)
        {
          return;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, src.ref);
        ref = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF (OBJ FROM OBJ): %d\n", ref);
      }

//      // move ctor
//      object(object&&)
//      {
//printf("MOVE!\n");
//      }

      object(const from_stack &s)
      {
//printf("FROM STACK\n");
        L = s.L;
        ref = LUA_REFNIL;
        if (L)
        {
          lua_pushvalue(L, s.pos);
          ref = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF (FROM STACK): %d\n", ref);
//printf("TYPE: %s\n", typestr());
        }
      }

      object(const proxy &p)
      {
//printf("FROM PROXY!\n");
        L = p.L;
        ref = LUA_REFNIL;
        if (L)
        {
          if (p.ref_table == LUA_REFNIL)
          {
            luaL_error(L, "[%s:%d] attempt to index a nil value", __FILE__, __LINE__);
          }
          lua_rawgeti(L, LUA_REGISTRYINDEX, p.ref_table);
//printf("TABLE REF: %d\n", p.ref_table);
//printf("TABLE TYPE: %d\n", lua_type(L, -1));
          lua_rawgeti(L, LUA_REGISTRYINDEX, p.ref_key);
//printf("KEY REF: %d\n", p.ref_key);
//printf("KEY TYPE: %d\n", lua_type(L, -1));
          lua_gettable(L, -2);
//printf("THIS TYPE: %d\n", lua_type(L, -1));
          ref = luaL_ref(L, LUA_REGISTRYINDEX);
          lua_pop(L, 1);
//printf("REF (OBJ FROM P): %d\n", ref);
        }
//printf("THIS TYPE2: %d\n", type());
      }

      template <typename T>
        object(lua_State *L, const T &val)
        : L(L), ref(LUA_REFNIL)
      {
        if (L)
        {
          object::push(L, val);
          ref = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF (OBJ FROM T): %d\n", ref);
//printf("TYPE: %s\n", lua_typename(L, type()));
        }
      }

      ~object()
      {
        clear();
      }

      bool clear()
      {
        if (L)
        {
//printf("UNREF (OBJECT): %d\n", ref);
          luaL_unref(L, LUA_REGISTRYINDEX, ref);
          ref = LUA_REFNIL;
        }
        return true;
      }

      const char *c_str() const
      {
        const char *c_str = luaL_tolstring(L, -1, NULL);
        lua_pop(L, 1);
        return c_str;
      }

      object getmetatable() const
      {
        this->push();
        if (! lua_getmetatable(L, -1))
        {
          lua_pop(L, 1);
          return object();
        }
        object m = from_stack(L, -1);
        lua_pop(L, 2);
        return m;
      }

      lua_State *interpreter() const
      {
        return L;
      }

      bool is_table() const
      {
        if ( this->type() == LUA_TTABLE ) { return true; }
        return false;
      }

      bool is_valid() const
      {
        if (! L) { return false; }
        if (ref == LUA_REFNIL) { return false; }
        return true;
      }

      // member push
      bool push() const
      {
        if (! L) { return false; }
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        return true;
      }
      bool push(lua_State *L) const
      {
        if (! L) { return false; }
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        return true;
      }

      bool setmetatable(const object &t)
      {
        this->push();
        t.push();
        lua_setmetatable(L, -2);
        lua_pop(L, 2);
        return true;
      }

      template <typename T>
        object table(const T &index)
      {
//printf("TABLE!\n");
//printf("TYPE: %d\n", (*this)[index].obj().type());
        object t = (*this)[index];
//printf("TYPE of T: %d\n", t.type());
        if (t.type() == LUA_TTABLE) { return t; }
        if (t.type() == LUA_TNIL)
        {
          t = newtable(L);
//printf("T TYPE: %d\n", t.type());
          (*this)[index] = newtable(L);
//printf("TYPE: %d\n", (*this)[index].type());
          return (*this)[index];
        }
        fprintf(stderr, "error: '%s' is not table\n", (const char *)t);
        return object();
      }

      std::string tostring() const
      {
        size_t len;
        if (! L) { return ""; }
        this->push();
        const char *c_str = luaL_tolstring(L, -1, &len);
//printf("LEN: %d\n", int(len));
//printf("STR: %s\n", str);
        std::string str;
        if (c_str)
        {
          str.assign(c_str, 0, len);
        }
        lua_pop(L, 2);
        return str;
      }


      int type() const
      {
        if (ref == LUA_REFNIL) { return LUA_TNIL; }
        this->push();
        int t = lua_type(L, -1);
        lua_pop(L, 1);
        return t;
      }

      const char *typestr() const
      {
        return lua_typename(L, type());
      }


      template <typename T>
        proxy operator[](const T &key)
      {
        this->push();
//printf("[] REF: %d\n", ref);
//printf("[] TYPE: %d\n", lua_type(L, -1));
        int ref_table = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF (TABLE): %d\n", ref_table);
//printf("[] TYPE2: %d\n", type());
        object::push(L, key);
        int ref_key = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF (KEY): %d\n", ref_key);
//printf("[] TYPE3: %d\n", type());
        return proxy(L, ref_table, ref_key);
      }

      object operator()()
      {
        this->push();
        lua_call(L, 0, 1);
        object result = from_stack(L, -1);
        lua_pop(L, 1);
        return result;
      }
      template <typename T1>
        object operator()(const T1 &arg1)
      {
        this->push();
        object::push(L, arg1);
        lua_call(L, 1, 1);
        object result = from_stack(L, -1);
        lua_pop(L, 1);
        return result;
      }
      template <typename T1, typename T2>
        object operator()(const T1 &arg1, const T2 &arg2)
      {
        this->push();
        object::push(L, arg1);
        object::push(L, arg2);
        lua_call(L, 2, 1);
        object result = from_stack(L, -1);
        lua_pop(L, 1);
        return result;
      }

      operator bool() const
      {
        if (! is_valid()) { return false; }
        this->push();
        bool result = lua_toboolean(L, -1);
        lua_pop(L, 1);
        return result;
      }

      operator const char *() const
      {
        if (! L) { return NULL; }
        this->push();
//        const char *str = lua_tostring(L, -1);
        const char *str = luaL_tolstring(L, -1, NULL);
//        lua_pop(L, 1);
        lua_pop(L, 2);
        return str;
      }

      object& operator=(const object &src)
      {
//printf("ASSIGN!\n");
        clear();
        L = src.L;
        if (! L)
        {
          ref = LUA_REFNIL;
          return *this;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, src.ref);
        ref = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF (OBJ ASSIGN): %d\n", ref);
        return *this;
      }

      bool operator==(const object &rhs)
      {
        this->push();
        rhs.push();
        bool result = lua_rawequal(L, -2, -1);
        lua_pop(L, 2);
        return result;
      }
      bool operator!=(const object &rhs)
      {
        return !(*this == rhs);
      }

      // static functions
      static bool push(lua_State *L, const bool &value);
      static bool push(lua_State *L, const char *val);
      static bool push(lua_State *L, const int &value);
      static bool push(lua_State *L, const double &value);
      static bool push(lua_State *L, const lua_CFunction &value);
      static bool push(lua_State *L, const std::string &value);
      static bool push(lua_State *L, const object &value);
      static bool push(lua_State *L, const proxy &value);
      template <typename T>
        static bool push(lua_State *L, T *val);
      static object newtable(lua_State *L);

    private:
      lua_State *L;
      int ref;
  };
  typedef object::from_stack from_stack;

  template <typename T>
    class managed
  {
    public:
      managed(T *p, bool adopt)
        : p(p), adopt(adopt) { }
      virtual ~managed()
      {
        printf("RELEASE UDATA!\n");
        if (adopt)
        {
          printf("RELEASE THE INSTANCE\n");
          delete p;
        }
      }

      // placement new
      static void* operator new(std::size_t, lua_State *L)
      {
//printf("PLACEMENT NEW!\n");
        return lua_newuserdata(L, sizeof(managed<T>));
      }

      T *p;
      bool adopt;
  };

  template <typename T>
    class finalizer
  {
    public:
      static int lfunc(lua_State *L)
      {
        printf("NO RELEASE!\n");
        return 0;
      }
  };
  template <typename T>
    class finalizer<T *>
  {
    public:
      static int lfunc(lua_State *L)
      {
        printf("RELEASING!\n");
        managed<T> *u = (managed<T>*)lua_touserdata(L, 1);
        // call only the dtor (not delete)
        u->~managed();
        // lua will release the memory
        return 0;
      }
  };

//  template <typename T, T *arg>
  template <typename T, T arg>
    struct cfunc_hold;

  template <typename T>
    struct functype_hold
  {
    template <T func>
      lua_CFunction get_lfunc()
    {
      return cfunc_hold<T, func>::lfunc;
    }

    static T cast_from(const object &obj);
  };

  template <typename T>
    inline functype_hold<T*> get_functype(T *func)
  {
    return functype_hold<T*>();
  }
  template <typename C, typename T>
    inline functype_hold<T (C::*)> get_functype(T (C::*func))
  {
    return functype_hold<T (C::*)>();
  }

  #define function(func) get_functype(func).get_lfunc<func>()
  #define method(func) get_functype(&func).get_lfunc<&func>()

  template <typename T>
    class casttype;

  template <typename T>
    inline T object_cast(const object &obj)
  {
    return casttype<T>::cast(obj);
  }

  template <typename From, typename To>
    inline void* downcast(void *from)
  {
    From *f = (From *)from;
printf("CAST FROM: %p\n", f);
    To *to = f;
printf("CAST TO: %p\n", to);
    return to;
  }

  inline int get_member(lua_State *L)
  {
    // ins (arg1) is instance
    // field (arg2) is field name
    // meta (stack3) = getmetatable(ins)
    lua_getmetatable(L, 1);
    // mem (stack4) = rawget(meta, "members")
    lua_pushstring(L, "members");
    lua_rawget(L, 3);
    // if type(mem) == "table" then
    if (lua_type(L, 4) == LUA_TTABLE)
    {
      // val (stack5) = rawget(mem, field)
      lua_pushvalue(L, 2);
      lua_rawget(L, 4);
      // if val ~= nil
      if (! lua_isnil(L, 5))
      {
        // return val
        lua_replace(L, 3);
        lua_pop(L, 1);
        return 1;
      }
      lua_pop(L, 1);
      // stack top -> 4
    }
    // class (stack5) = rawget(meta, "class")
    lua_pushstring(L, "class");
    lua_rawget(L, 3);
    if (lua_istable(L, 5))
    {
      // val (stack6) = class["get_" .. field]
      lua_pushstring(L, "get_");
      lua_pushvalue(L, 2);
      lua_concat(L, 2);
      lua_gettable(L, 5);
      // if val ~= nil then
      if (lua_isfunction(L, 6))
      {
        // val (stack6) = val(instance)
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        // return val
        lua_replace(L, 3);
        lua_pop(L, 2);
        return 1;
      }
      // val (stack7) = class[field]
      lua_pushvalue(L, 2);
      lua_gettable(L, 5);
      // return val
      lua_replace(L, 3);
      lua_pop(L, 3);
      return 1;
    }
    return 0;
  }

  inline int set_member(lua_State *L)
  {
    // ins (arg1) is instance
    // field (arg2) is field name
    // val (arg3) is value
printf("__NEWINDEX\n");
    // meta (stack4) = getmetatable(ins)
    lua_getmetatable(L, 1);
    // class (stack5) = rawget(meta, "class")
    lua_pushstring(L, "class");
    lua_rawget(L, 4);
    // getter (stack6) = class["get_" .. field]
    lua_pushstring(L, "get_");
    lua_pushvalue(L, 2);
    lua_concat(L, 2);
    lua_gettable(L, 5);
    // setter (stack7) = class["set_" .. field]
    lua_pushstring(L, "set_");
    lua_pushvalue(L, 2);
    lua_concat(L, 2);
    lua_gettable(L, 5);
    // if type(getter) == 'function' and type(setter) ~= 'function' then
    if (lua_isfunction(L, 6) && ! lua_isfunction(L, 7) )
    {
      // field (stack8) = tostring(arg2)
      const char *field = luaL_tolstring(L, 2, NULL);
      luaL_error(L, "method: set_%s is not defined", field);
      lua_pop(L, 6);
      return 0;
    }
    // if type(setter) == 'function' then
    if (lua_isfunction(L, 7))
    {
printf("CLASS SETTER\n");
      // setter(ins, val)
      lua_pushvalue(L, 1);
      lua_pushvalue(L, 3);
      lua_call(L, 2, 0);
      // stack top -> 6
      // return
      lua_pop(L, 4);
      return 0;
    }
printf("INSTANCE MEMBER\n");
    // mem (arg8) = rawget(meta, "members")
    lua_pushstring(L, "members");
    lua_rawget(L, 4);
    // mem[field] = val
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_rawset(L, 8);
    // stck top -> 7
    // return
    lua_pop(L, 5);
    return 0;
  }

  // declarations

  inline object globals(lua_State *L)
  {
//printf("GLOBALS!\n");
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
    object g = from_stack(L, -1);
    lua_pop(L, 1);
    return g;
  }

  inline object load(lua_State *L, const std::string &str)
  {
    if (luaL_loadstring(L, str.c_str()) == LUA_OK)
    {
      object f = from_stack(L, -1);
      lua_pop(L, 1);
      return f;
    }
    return object();
  }

  template <typename T>
    inline object lightuserdata(lua_State *L, T *p)
  {
    lua_pushlightuserdata(L, (void *)p);
    object o = from_stack(L, -1);
    lua_pop(L, 1);
    return o;
  }

  inline std::string get_args_string(lua_State *L)
  {
    int nargs = lua_gettop(L);
//    printf("NUM ARGS: %d\n", nargs);
    std::string args = "(";
    for (int i = 1; i <= nargs; i++)
    {
      if (i > 1) { args += ", "; }
      int type = lua_type(L, i);
      if (type == LUA_TUSERDATA)
      {
        object m = object(from_stack(L, i)).getmetatable();
        if (! m["luaport"].is_valid())
        {
          args += "<unknown udata>";
        }
        else
        {
          object class_to_name = registry(L)["luaport"]["class_to_name"];
          object name = class_to_name[m["class"]];
          args += (const char *)name;
        }
      }
      else
      {
        args += lua_typename(L, type);
      }
    }
    args += ")";
    return args;
  }

  // lua push (generic)
  inline bool object::push(lua_State *L, const bool &val)
  {
printf("PUSH BOOL: %d\n", (int)val);
    if (! L) { return false; }
    lua_pushboolean(L, val);
    return true;
  }
  inline bool object::push(lua_State *L, const char *val)
  {
//printf("PUSH: \"%s\"\n", val);
    if (! L) { return false; }
    lua_pushstring(L, val);
    return true;
  }
  inline bool object::push(lua_State *L, const double &val)
  {
//printf("PUSHING! %lf\n", val);
    if (! L) { return false; }
    lua_pushnumber(L, val);
    return true;
  }
  inline bool object::push(lua_State *L, const int &val)
  {
//printf("PUSH INT: %d\n", val);
    if (! L) { return false; }
    lua_pushinteger(L, val);
    return true;
  }
  inline bool object::push(lua_State *L, const lua_CFunction &val)
  {
//printf("PUSH CFUNC: %p\n", val);
    if (! L) { return false; }
    lua_pushcfunction(L, val);
    return true;
  }
  inline bool object::push(lua_State *L, const object &val)
  {
    return val.push(L);
  }
  inline bool object::push(lua_State *L, const proxy &val)
  {
    return object(val).push(L);
  }
  inline bool object::push(lua_State *L, const std::string &val)
  {
    if (! L) { return false; }
//printf("PUSHED: %s\n", val.c_str());
//    lua_pushstring(L, val.c_str());
    lua_pushlstring(L, val.data(), val.length());
    return true;
  }
  template <typename T>
    inline bool object::push(lua_State *L, T *val)
  {
    if (! L) { return false; }
printf("PUSH UDATA: %p\n", val);
    managed<T> *u = new(L) managed<T>(val, true);
    object c = get_class<T>(L);
    if (! c.is_valid())
    {
      luaL_error(L, "unregisted class: %s\n", typeid(T).name());
      return false;
    }
    object m = newtable(L);
    m["class"] = c;
    m["luaport"] = true;
    m["members"] = newtable(L);
    m["__gc"] = finalizer<managed<T>*>::lfunc;
    m["__index"] = get_member;
    m["__newindex"] = set_member;
    m.push();
    lua_setmetatable(L, -2);
printf("TYPE: %s\n", lua_typename(L, lua_type(L, -1)));
    return true;
  }

  inline static object registry(lua_State *L)
  {
    lua_pushnil(L);
    lua_copy(L, LUA_REGISTRYINDEX, -1);
    object r = from_stack(L, -1);
    lua_pop(L, 1);
    return r;
  }

  inline object object::newtable(lua_State *L)
  {
    lua_newtable(L);
    object t(from_stack(L, -1));
    lua_pop(L, 1);
    return t;
  }
  static object (*newtable)(lua_State *L) = object::newtable;

  template <typename T>
    inline object get_class(lua_State *L)
  {
    object func_to_class = registry(L)["luaport"]["func_to_class"];
    return func_to_class[finalizer<managed<T>*>::lfunc];
  }

  template <typename T>
    inline static object newclass(lua_State *L, const std::string &name)
  {
    try {
      object c = newtable(L);
      object class_to_func = registry(L)["luaport"]["class_to_func"];
      class_to_func[c] = finalizer<managed<T>*>::lfunc;
      object class_to_name = registry(L)["luaport"]["class_to_name"];
      class_to_name[c] = name;
      object func_to_class = registry(L)["luaport"]["func_to_class"];
      func_to_class[finalizer<managed<T>*>::lfunc] = c;
      object func_to_name = registry(L)["luaport"]["func_to_name"];
      func_to_name[finalizer<managed<T>*>::lfunc] = name;
      return c;
    }
    catch (...) {
      fprintf(stderr, "error on luaport::luaclass\n");
      return object();
    }
  }
  template <typename D, typename B>
    inline static object newclass(lua_State *L, const std::string &name)
  {
    object b = get_class<B>(L);
    object d = newclass<D>(L, name);
    object m = newtable(L);
    m["__index"] = b;
    m["downcast"] = lightuserdata(L, downcast<D, B>);
    d.setmetatable(m);
    return d;
  }

  template <typename T>
    inline const char *get_typename(lua_State *L)
  {
    object fmap = registry(L)["luaport"]["func_to_name"];
    object str = fmap[finalizer<T>::lfunc];
    if (str) { return str; }
    str = fmap[finalizer<managed<T>*>::lfunc];
    if (str) { return str; }
    return "[unknown]";
  }

  inline void open(lua_State *L)
  {
    object port = registry(L).table("luaport");
    object class_to_name = port.table("class_to_name");
    object class_to_func = port.table("class_to_func");
    object func_to_name = port.table("func_to_name");
    object func_to_class = port.table("func_to_class");

    func_to_name[finalizer<int>::lfunc] = "int";
    func_to_name[finalizer<float>::lfunc] = "float";
    func_to_name[finalizer<double>::lfunc] = "double";
    func_to_name[finalizer<std::string>::lfunc] = "string";
  }

  // template specifications

  template <void (*f)()>
//    struct cfunc_hold<void (), f>
    struct cfunc_hold<void (*)(), f>
  {
    static int lfunc(lua_State *L)
    {
      printf("void FUNC(): %p\n", f);
      return 0;
    }
  };

  template <typename T0, T0 (*f)()>
//    struct cfunc_hold<T0 (), f>
    struct cfunc_hold<T0 (*)(), f>
  {
    static int lfunc(lua_State *L)
    {
      printf("T0 FUNC(): %p\n", f);
      return 0;
    }
  };

  template <typename C, typename T0, T0 (C::*m)()>
    struct cfunc_hold<T0 (C::*)(), m>
  {
    static std::string signature(lua_State *L)
    {
      std::string s;
      s += get_typename<T0>(L);
      s += " (";
      s += get_typename<C>(L);
      s += "::*)()";
      return s;
    }

    static int lfunc(lua_State *L)
    {
      try {
        C *c = object_cast<C *>(object(from_stack(L, 1)));
        object::push(L, (c->*m)() );
        return 1;
      }
      catch (...) {
        luaL_error(L, "signature: %s\nbut got: %s\n", signature(L).c_str(), get_args_string(L).c_str());
      }
      return 0;
    }
  };

  template <typename T1, void (*f)(T1)>
//    struct cfunc_hold<void (T1), f>
    struct cfunc_hold<void (*)(T1), f>
  {
    static int lfunc(lua_State *L)
    {
//      printf("void FUNC(T1): %p\n", f);
      printf("void (%s)\n", get_typename<T1>(L));
      return 0;
    }
  };

  template <typename T0, typename T1, T0 (*f)(T1)>
//    struct cfunc_hold<T0 (T1), f>
    struct cfunc_hold<T0 (*)(T1), f>
  {
    static int lfunc(lua_State *L)
    {
//      printf("T0 FUNC(T1): %p\n", f);
      printf("%s (%s)\n", get_typename<T0>(L), get_typename<T1>(L));
      return 0;
    }
  };

  template <>
    struct casttype<lua_CFunction>
  {
    static lua_CFunction cast(const object &obj)
    {
printf("CAST TO CFUNCTION\n");
      lua_State *L = obj.interpreter();
      obj.push();
      lua_CFunction f = lua_tocfunction(L, -1);
      lua_pop(L, 1);
      return f;
    }
  };

  template <typename T>
    bool is_typeof(const object &obj)
  {
    lua_State *L = obj.interpreter();
    object m = obj.getmetatable();
    if (! m.is_table()) { return false; }
    if (! m["luaport"].is_valid()) { return false; }

    object c = m["class"];
    for (;;)
    {
printf("%s?\n", (const char *)registry(L)["luaport"]["class_to_name"][c].obj());
      if (c == get_class<T>(L)) { return true; }
      m = c.getmetatable();
      if (m.type() != LUA_TTABLE) { return false; }
      c = m["__index"];
    }
    return false;
  }

  template <typename T>
    struct casttype<T *>
  {
    static T* cast(const object &obj)
    {
printf("CASTING!\n");
      lua_State *L = obj.interpreter();
      object m = obj.getmetatable();
      if (! m.is_table()) { throw std::bad_alloc(); }
      if (! m["luaport"].is_valid()) { throw std::bad_alloc(); }

      object c = m["class"];
      obj.push();
      managed<void> *u = (managed<void> *)lua_touserdata(L, -1);
      void *p = u->p;
      lua_pop(L, 1);
      for (;;)
      {
printf("%s?\n", (const char *)registry(L)["luaport"]["class_to_name"][c].obj());
        if (c == get_class<T>(L)) { break; }
        m = c.getmetatable();
        if (m.type() != LUA_TTABLE) { throw std::bad_alloc(); }
        c = m["__index"];
        object(m["downcast"]).push();
        void*(*cast)(void *) = (void* (*)(void*))lua_touserdata(L, -1);
        lua_pop(L, 1);
        p = cast(p);
      }
      return (T *)p;
    }
  };

}

#endif // _LUAPORT_HPP
