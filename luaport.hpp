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
#include <string>
#include <typeinfo>

namespace luaport
{

  // constants
  const bool adopt = true;

  // function prototypes
  extern void open(lua_State *L);

  template <typename T>
    extern class object get_class(lua_State *L);
  template <typename T>
    extern std::string get_typename(lua_State *L);
  extern class object globals(lua_State *L);
  template <typename T>
    extern object lightuserdata(lua_State *L, T *p);
  extern class object load(lua_State *L, const std::string &str);
  template <typename T>
    extern class object newclass(lua_State *L, const std::string &name);
  template <typename Derived, typename Base>
    extern class object newclass(lua_State *L, const std::string &name);
  extern class object newtable(lua_State *L);
  template <typename T>
    extern T object_cast(const object &obj);
  extern class object registry(lua_State *L);

  extern int lua_newclass(lua_State *L);

  extern void push(lua_State *L, const bool &value);
  extern void push(lua_State *L, const char *val);
  extern void push(lua_State *L, const int &value);
  extern void push(lua_State *L, const double &value);
  extern void push(lua_State *L, const lua_CFunction &value);
  extern void push(lua_State *L, const std::string &value);
  extern void push(lua_State *L, const class object &value);
  extern void push(lua_State *L, const class proxy &value);
  template <typename T>
    extern void push(lua_State *L, T *val);

  #define function(func) get_functype(func).get_lfunc<func>()
  #define method(func) get_functype(&func).get_lfunc<&func>()


  // class definitions

  class exception : public std::exception
  {
    public:
      exception(const std::string &msg)
        : std::exception(), msg(msg)
      { }

      virtual const char *what() const throw() { return msg.c_str(); }

      virtual ~exception() throw() { }

    private:
      std::string msg;
  };


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


  class iterator
  {
    public:
      iterator(const class object &table);

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

      class object key() const;
      class object value() const;

      operator bool() const
      {
        return is_valid();
      }

      class proxy operator*() const;

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


  class object
  {
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

      object(const class proxy &p);

      template <typename T>
        object(lua_State *L, const T &val)
        : L(L), ref(LUA_REFNIL)
      {
        if (L)
        {
          luaport::push(L, val);
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

      bool is_class() const;
      bool is_instance() const;

      bool is_table() const
      {
        if ( this->type() == LUA_TTABLE ) { return true; }
        return false;
      }

      template <typename T>
        bool is_typeof();

      bool is_valid() const
      {
        if (! L) { return false; }
        if (ref == LUA_REFNIL) { return false; }
        return true;
      }

      // member push
      void push() const
      {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
      }
      void push(lua_State *L) const
      {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
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
        class proxy operator[](const T &key);

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
        luaport::push(L, arg1);
        lua_call(L, 1, 1);
        object result = from_stack(L, -1);
        lua_pop(L, 1);
        return result;
      }
      template <typename T1, typename T2>
        object operator()(const T1 &arg1, const T2 &arg2)
      {
        this->push();
        luaport::push(L, arg1);
        luaport::push(L, arg2);
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

    private:
      lua_State *L;
      int ref;
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
        luaport::push(L, val);
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
        luaport::push(L, key);
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


  // inner class defintion

  template <typename T>
    struct _casttype;

  template <typename T, T arg>
    struct _cfunc_hold;


  template <typename T>
    struct _finalizer
  {
    static int lfunc(lua_State *L);
  };
  template <typename T>
    struct _finalizer<T *>
  {
    static int lfunc(lua_State *L);
  };


  template <typename T>
    struct _functype_hold
  {
    template <T func>
      lua_CFunction get_lfunc()
    {
      return _cfunc_hold<T, func>::lfunc;
    }
  };


  template <typename T>
    class _managed
  {
    public:
      _managed(T *p, bool adopt)
        : p(p), adopt(adopt) { }
      virtual ~_managed()
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
        return lua_newuserdata(L, sizeof(_managed<T>));
      }

      T *p;
      bool adopt;
  };


  // -----------------------------------------------------------------------
  // inner (static) function implementation


  template <typename R>
    inline static int _call_and_return(lua_State *L, R (*f)() )
  {
    luaport::push(L, (*f)()) ;
    return 1;
  }
  inline static int _call_and_return(lua_State *L, void (*f)() )
  {
    (*f)();
    return 0;
  }
  template <typename R, typename C>
    inline static int _call_and_return(lua_State *L, R (C::*m)(), C *c)
  {
    luaport::push(L, (c->*m)() );
    return 1;
  }
  template <typename C>
    inline static int int_call_and_return(lua_State *L, void (C::*m)(), C *c)
  {
    (c->*m)();
    return 0;
  }
  template <typename R, typename T1>
    inline static int _call_and_return(lua_State *L, R (*f)(T1), T1 a1)
  {
    luaport::push(L, (*f)(a1) );
    return 1;
  }
  template <typename R, typename T1>
    inline static int _call_and_return(lua_State *L, void (*f)(T1), T1 a1)
  {
    (*f)(a1);
    return 0;
  }
  template <typename R, typename C, typename T1>
    inline static int _call_and_return(lua_State *L, R (C::*m)(T1), C *c, T1 a1)
  {
    luaport::push(L, (c->*m)(a1) );
    return 1;
  }
  template <typename R, typename C, typename T1>
    inline static int _call_and_return(lua_State *L, void (C::*m)(T1), C *c, T1 a1)
  {
    (c->*m)(a1);
    return 0;
  }


  template <typename From, typename To>
    inline static void* _downcast(void *from)
  {
    From *f = (From *)from;
printf("CAST FROM: %p\n", f);
    To *to = f;
printf("CAST TO: %p\n", to);
    return to;
  }


  inline static std::string _get_args_string(lua_State *L)
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


  template <typename R>
    inline static std::string _get_signature(lua_State *L, R (*f)() )
  {
    return get_typename<R>(L) + " ()";
  }
  template <typename R, typename C>
    inline static std::string _get_signature(lua_State *L, R (C::*m)() )
  {
    return get_typename<R>(L) + " (" + get_typename<C>(L) + "::*)()";
  }
  template <typename R, typename T1>
    inline static std::string _get_signature(lua_State *L, R (*f)(T1) )
  {
    return get_typename<R>(L) + " (" + get_typename<T1>(L) + ")";
  }
  template <typename R, typename C, typename T1>
    inline static std::string _get_signature(lua_State *L, R (C::*m)(T1) )
  {
    return get_typename<R>(L) + " (" + get_typename<C>(L) + "::*)(" + get_typename<T1>(L) + ")";
  }


  inline static int _lua_class_get_member(lua_State *L)
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

  inline static int _lua_class_set_member(lua_State *L)
  {
    // ins (arg1) is instance
    // field (arg2) is field name
    // val (arg3) is value
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
      // setter(ins, val)
      lua_pushvalue(L, 1);
      lua_pushvalue(L, 3);
      lua_call(L, 2, 0);
      // stack top -> 6
      // return
      lua_pop(L, 4);
      return 0;
    }
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


  inline static int _lua_class_create(lua_State *L)
  {
    printf("CALL!\n");
    luaL_checktype(L, 1, LUA_TTABLE);
    object c = from_stack(L, 1);
    object init = c["__init"];
    if (init.type() != LUA_TFUNCTION)
    {
      luaL_error(L, "__init method is not defined\n");
      return 0;
    }

    lua_newuserdata(L, 0);
    object m = newtable(L);
    m["class"] = c;
    m["luaport"] = true;
    m["members"] = newtable(L);
    m["__gc"] = _finalizer<void>::lfunc;
    m["__index"] = _lua_class_get_member;
    m["__newindex"] = _lua_class_set_member;
    m.push();
    lua_setmetatable(L, -2);

    object u = from_stack(L, -1);
    printf("U: %s\n", (const char *)u);
    init(u);
    return 1;
  }

  inline static void _lua_error_signature(lua_State *L, const std::string &sig)
  {
    std::string msg;
    msg += "signature: " + sig + "\n";
    msg += "but got: " + _get_args_string(L);
    luaL_error(L, "%s", msg.c_str());
  }


  // -----------------------------------------------------
  // innner class implementation


  template <>
    struct _casttype<double>
  {
    static double cast(const object &obj)
    {
printf("CAST TO DOUBLE\n");
      lua_State *L = obj.interpreter();
      obj.push();
      double d = lua_tonumber(L, -1);
      lua_pop(L, 1);
      return d;
    }
  };
  template <>
    struct _casttype<lua_CFunction>
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
    struct _casttype<T *>
  {
    static T* cast(const object &obj)
    {
printf("CASTING!\n");
      lua_State *L = obj.interpreter();
      object m = obj.getmetatable();
      if (! m.is_table()) { throw std::bad_cast(); }
      if (! m["luaport"].is_valid()) { throw std::bad_cast(); }

      object c = m["class"];
      obj.push();
      _managed<void> *u = (_managed<void> *)lua_touserdata(L, -1);
      void *p = u->p;
      lua_pop(L, 1);
      for (;;)
      {
printf("%s?\n", (const char *)registry(L)["luaport"]["class_to_name"][c].obj());
        if (c == get_class<T>(L)) { break; }
        m = c.getmetatable();
        if (m.type() != LUA_TTABLE) { throw std::bad_cast(); }
        c = m["__index"];
        object(m["downcast"]).push();
        void*(*cast)(void *) = (void* (*)(void*))lua_touserdata(L, -1);
        lua_pop(L, 1);
        if (cast)
        {
printf("YES CASTING\n");
          p = cast(p);
        }
        else
        {
printf("NO CASTING\n");
        }
      }
      return (T *)p;
    }
  };


  template <typename R, R (*f)()>
    struct _cfunc_hold<R (*)(), f>
  {
    static int lfunc(lua_State *L)
    {
      try {
        return _call_and_return(L, f);
      }
      catch (...) {
        _lua_error_signature(L, _get_signature(L, f) );
      }
      return 0;
    }
  };
  template <typename R, typename C, R (C::*m)()>
    struct _cfunc_hold<R (C::*)(), m>
  {
    static int lfunc(lua_State *L)
    {
      try {
        C *c = object_cast<C *>(object(from_stack(L, 1)));
        return _call_and_return(L, m, c);
      }
      catch (...) {
        _lua_error_signature(L, _get_signature(L, m) );
      }
      return 0;
    }
  };

  template <typename R, typename T1, R (*f)(T1)>
    struct _cfunc_hold<R (*)(T1), f>
  {
    static int lfunc(lua_State *L)
    {
      try {
        T1 arg1 = object_cast<T1>(object(from_stack(L, 1)));
        return _call_and_return(L, f, arg1);
      }
      catch (...) {
        _lua_error_signature(L, _get_signature(L, f));
      }
      return 0;
    }
  };
  template <typename R, typename C, typename T1, R (C::*m)(T1)>
    struct _cfunc_hold<R (C::*)(T1), m>
  {
    static int lfunc(lua_State *L)
    {
      try {
        C *c = object_cast<C *>(object(from_stack(L, 1)) );
        T1 arg1 = object_cast<T1>(object(from_stack(L, 2)) );
        return _call_and_return(L, m, c, arg1);
      }
      catch (...) {
        _lua_error_signature(L, _get_signature(L, m));
      }
      return 0;
    }
  };



  template <typename T>
    inline int _finalizer<T>::lfunc(lua_State *L)
  {
    object u = from_stack(L, 1);
    object f = u["__finalize"];
    if (f.type() == LUA_TFUNCTION)
    {
      f(u);
    }
    // lua will release the memory
    return 0;
  }
  template <typename T>
    inline int _finalizer<T *>::lfunc(lua_State *L)
  {
    _managed<T> *u = (_managed<T>*)lua_touserdata(L, 1);
    object inst = from_stack(L, 1);
    object f = inst["__finalize"];
    if (f.type() == LUA_TFUNCTION)
    {
      f(inst);
    }
    // call only the dtor (not delete)
    u->~_managed();
    // lua will release the memory
    return 0;
  }


  // ------------------------------------------------------
  // function implementation

  template <typename T>
    inline _functype_hold<T*> get_functype(T *func)
  {
    return _functype_hold<T*>();
  }
  template <typename C, typename T>
    inline _functype_hold<T (C::*)> get_functype(T (C::*func))
  {
    return _functype_hold<T (C::*)>();
  }


  template <typename T>
    inline std::string get_typename(lua_State *L)
  {
    object fmap = registry(L)["luaport"]["func_to_name"];
    object str = fmap[_finalizer<T>::lfunc];
    if (str) { return str.tostring(); }
    str = fmap[_finalizer<_managed<T>*>::lfunc];
    if (str) { return str.tostring(); }
    return "[unknown]";
  }


  template <typename T>
    inline object get_class(lua_State *L)
  {
    object func_to_class = registry(L)["luaport"]["func_to_class"];
    return func_to_class[_finalizer<_managed<T>*>::lfunc];
  }


  inline object globals(lua_State *L)
  {
//printf("GLOBALS!\n");
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
    object g = from_stack(L, -1);
    lua_pop(L, 1);
    return g;
  }


  template <typename T>
    inline object lightuserdata(lua_State *L, T *p)
  {
    lua_pushlightuserdata(L, (void *)p);
    object o = from_stack(L, -1);
    lua_pop(L, 1);
    return o;
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


  inline int lua_newclass(lua_State *L)
  {
    const char *name = luaL_checkstring(L, 1);

    try {
      object c = newtable(L);
      object m = newtable(L);
      object class_to_func = registry(L)["luaport"]["class_to_func"];
      class_to_func[c] = _finalizer<void>::lfunc;
      object class_to_name = registry(L)["luaport"]["class_to_name"];
      class_to_name[c] = name;
      object name_to_class = registry(L)["luaport"]["name_to_class"];
      name_to_class[name] = c;

      m["__call"] = _lua_class_create;
      c.setmetatable(m);
      c.push();
      return 1;
    }
    catch (...) {
      throw luaport::exception("error on class creation");
    }
    return 0;
  }


  template <typename T>
    inline object newclass(lua_State *L, const std::string &name)
  {
    try {
      object c = newtable(L);
      object m = newtable(L);
      object class_to_func = registry(L)["luaport"]["class_to_func"];
      class_to_func[c] = _finalizer<_managed<T>*>::lfunc;
      object class_to_name = registry(L)["luaport"]["class_to_name"];
      class_to_name[c] = name;
      object func_to_class = registry(L)["luaport"]["func_to_class"];
      func_to_class[_finalizer<_managed<T>*>::lfunc] = c;
      object func_to_name = registry(L)["luaport"]["func_to_name"];
      func_to_name[_finalizer<_managed<T>*>::lfunc] = name;
      object name_to_class = registry(L)["luaport"]["name_to_class"];
      name_to_class[name] = c;
      c.setmetatable(m);
      return c;
    }
    catch (...) {
      throw luaport::exception("error on luaport::newclass");
    }
    return object();
  }
  template <typename Derived, typename Base>
    inline object newclass(lua_State *L, const std::string &name)
  {
    object b = get_class<Base>(L);
    object d = newclass<Derived>(L, name);
    object m = d.getmetatable();
    m["__index"] = b;
    m["downcast"] = lightuserdata(L, _downcast<Derived, Base>);
    return d;
  }


  inline object newtable(lua_State *L)
  {
    lua_newtable(L);
    object t(from_stack(L, -1));
    lua_pop(L, 1);
    return t;
  }


  template <typename T>
    inline T object_cast(const object &obj)
  {
    return _casttype<T>::cast(obj);
  }


  inline void open(lua_State *L)
  {
    object port = registry(L).table("luaport");
    object class_to_name = port.table("class_to_name");
    object class_to_func = port.table("class_to_func");
    object func_to_name = port.table("func_to_name");
    object func_to_class = port.table("func_to_class");
    object name_to_class = port.table("name_to_class");

    func_to_name[_finalizer<int>::lfunc] = "int";
    func_to_name[_finalizer<float>::lfunc] = "float";
    func_to_name[_finalizer<double>::lfunc] = "double";
    func_to_name[_finalizer<std::string>::lfunc] = "string";

    globals(L)["class"] = lua_newclass;
  }


  inline void push(lua_State *L, const bool &val)
  {
    lua_pushboolean(L, val);
  }
  inline void push(lua_State *L, const char *val)
  {
    lua_pushstring(L, val);
  }
  inline void push(lua_State *L, const double &val)
  {
    lua_pushnumber(L, val);
  }
  inline void push(lua_State *L, const int &val)
  {
    lua_pushinteger(L, val);
  }
  inline void push(lua_State *L, const lua_CFunction &val)
  {
    lua_pushcfunction(L, val);
  }
  inline void push(lua_State *L, const object &val)
  {
    return val.push(L);
  }
  inline void push(lua_State *L, const proxy &val)
  {
    return object(val).push(L);
  }
  inline void push(lua_State *L, const std::string &val)
  {
    lua_pushlstring(L, val.data(), val.length());
  }
  template <typename T>
    inline void push(lua_State *L, T *val)
  {
printf("PUSH UDATA: %p\n", val);
    _managed<T> *u = new(L) _managed<T>(val, true);
    object c = get_class<T>(L);
    if (! c.is_valid())
    {
      std::string msg = "unregistered class: ";
      throw luaport::exception(msg + typeid(T).name());
    }
    object m = newtable(L);
    m["class"] = c;
    m["luaport"] = true;
    m["members"] = newtable(L);
    m["__gc"] = _finalizer<_managed<T>*>::lfunc;
    m["__index"] = _lua_class_get_member;
    m["__newindex"] = _lua_class_set_member;
    m.push();
    lua_setmetatable(L, -2);
printf("TYPE: %s\n", lua_typename(L, lua_type(L, -1)));
  }


  inline object registry(lua_State *L)
  {
    lua_pushnil(L);
    lua_copy(L, LUA_REGISTRYINDEX, -1);
    object r = from_stack(L, -1);
    lua_pop(L, 1);
    return r;
  }




  // ----------------------------------------------------------------
  // class implementation


  inline iterator::iterator(const object &table)
    : ref_table(LUA_REFNIL), ref_key(LUA_REFNIL), ref_val(LUA_REFNIL)
  {
    L = table.interpreter();
    if (! L) { throw luaport::exception("given invalid interpreter"); }
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


  inline object iterator::key() const
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_key);
    object key(from_stack(L, -1));
    lua_pop(L, 1);
    return key;
  }


  inline object iterator::value() const
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_val);
    object val(from_stack(L, -1));
    lua_pop(L, 1);
    return val;
  }


  inline proxy iterator::operator*() const
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_table);
    int t = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_key);
    int k = luaL_ref(L, LUA_REGISTRYINDEX);
    return proxy(L, t, k);
  }


  inline object::object(const proxy &p)
    : L(p.L), ref(LUA_REFNIL)
  {
//printf("FROM PROXY!\n");
    if (! L) { throw luaport::exception("given invalid interpreter"); }
    if (p.ref_table == LUA_REFNIL)
    {
      throw luaport::exception("attempt to index a nil value");
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, p.ref_table);
    lua_rawgeti(L, LUA_REGISTRYINDEX, p.ref_key);
    lua_gettable(L, -2);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1);
  }


  inline bool object::is_class() const
  {
printf("IS CLASS?\n");
    object name = registry(L)["luaport"]["class_to_name"][*this];
    return name.type() == LUA_TSTRING;
  }

  inline bool object::is_instance() const
  {
printf("IS INSTANCE?\n");
    object m = getmetatable();
    if (! m.is_table()) { return false; }
    if (! m["luaport"]) { return false; }
    return true;
  }


  template <typename T>
    inline bool object::is_typeof()
  {
    object c;
    object m;

    if (this->is_class())
    {
      c = *this;
    }
    else
    {
      m = getmetatable();
      if (! m.is_table()) { return false; }
      if (! m["luaport"].is_valid()) { return false; }
      c = m["class"];
    }

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
    inline proxy object::operator[](const T &key)
  {
    this->push();
    int ref_table = luaL_ref(L, LUA_REGISTRYINDEX);
    luaport::push(L, key);
    int ref_key = luaL_ref(L, LUA_REGISTRYINDEX);
    return proxy(L, ref_table, ref_key);
  }

}

#endif // _LUAPORT_HPP
