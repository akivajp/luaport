#ifndef _LUAPORT_HPP
#define _LUAPORT_HPP

/////////////////////////////////////////////////////////////////////////////
// Name:        luaport.hpp
// Purpose:     C++ <-> lua porting (binding) library
// Author:      Akiva Miura <akiva.miura@gmail.com>
// Modified by: spinor (@tplantd)
// Created:     03/18/2013
// Modified:    21/06/2013
// Copyright:   (C) 2013 Akiva Miura, spinor
// Licence:     MIT License
/////////////////////////////////////////////////////////////////////////////

#include <lua.hpp>
#include <string>
#include <typeinfo>
#include <cassert>

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
  extern int type(const class object &obj);
  extern class object registry(lua_State *L);

  #define function(func) get_functype(func).get_lfunc<func>()
  #define method(func) get_functype(&func).get_lfunc<&func>()


  // template class declarations
  namespace detail
  {
    template <typename T>
      class managed;
  }
  template <typename T>
    class reference;
  template <typename T>
    class weak_ref;


  // class definitions

  /// luaport exception class
  /**
   * thrown when exception occurs in using this library.
   */
  class exception : public std::exception
  {
    public:
      /// Constructor
      /**
       * @param msg : content of the exception
       */
      exception(const std::string &msg)
        : std::exception(), msg(msg)
      { }

      /// get the exception content
      /**
       * @return the exception content
       */
      virtual const char *what() const throw() { return msg.c_str(); }

      virtual ~exception() throw() { }

    private:
      std::string msg;
  };


  /// Lua stack index specifier
  /**
   * used when you want to initialize the lua object with a value from
   * the lua stack.
   */
  class from_stack
  {
    public:
      /// Cunstructor
      /**
       * @param L : lua interpreter
       * @param idx : index on the lua stack
       */
      from_stack(lua_State *L, int idx)
        : L(L), idx(idx)
      { }

    private:
      lua_State *L;
      int idx;
      friend class object;
  };


  class iterator
  {
    public:
      iterator(const class object &table);

      iterator(const iterator &src)
        : L(src.L)
      {
        assert(L != NULL);

        lua_rawgeti(L, LUA_REGISTRYINDEX, src.ref_table);
        ref_table = luaL_ref(L, LUA_REGISTRYINDEX);
        lua_rawgeti(L, LUA_REGISTRYINDEX, src.ref_key);
        ref_table = luaL_ref(L, LUA_REGISTRYINDEX);
        lua_rawgeti(L, LUA_REGISTRYINDEX, src.ref_val);
        ref_val = luaL_ref(L, LUA_REGISTRYINDEX);
      }

      ~iterator()
      {
        clear();
      }

      bool clear()
      {
        assert(L != NULL);

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
      iterator operator++(int)
      {
        iterator i = *this;
        ++(*this);
        return i;
      }

    private:
      lua_State *L;
      int ref_table;
      int ref_key;
      int ref_val;
  };


  /// Lua object class
  /**
   * refers to lua object.
   */
  class object
  {
    public:

      /// Constructor (nil object with lua interpreter)
      /**
       * @param L : lua interpreter
       */
      object(lua_State *L)
        : L(L), ref(LUA_REFNIL)
      {
      }

      /// Constructor (nil object)
      object()
        : L(NULL), ref(LUA_REFNIL)
      {
      }

      /// Copy Constructor
      /**
       * @param src : lua object to copy
       */
      object(const object &src)
        : L(src.L), ref(LUA_REFNIL)
      {
        if (! L)
        {
          // copying empty (nil) object
          return;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, src.ref);
        ref = luaL_ref(L, LUA_REGISTRYINDEX);
      }


//      // Move Constructor
//      object(object&&);


      /// Constructor (from stack)
      /**
       * @param s : spcifies index on the lua stack
       */
      object(const from_stack &s)
        : L(s.L), ref(LUA_REFNIL)
      {
        assert(L != NULL);

        lua_pushvalue(L, s.idx);
        ref = luaL_ref(L, LUA_REGISTRYINDEX);
      }


      /// Constructor (from proxy)
      /**
       * @param p : specifies pair binding of table and key
       */
      object(const class proxy &p);


      /// Constructor (elemental value object)
      /**
       * @param L : lua interpreter
       * @param val : elemental value
       */
      template <typename T>
        object(lua_State *L, const T &val);


      /// Constructor (string value object)
      /**
       * @param L : lua interpreter
       * @param str: string value
       */
      object(lua_State *L, const char *str);


      /// Constructor (object of registred class)
      /**
       * @param L : lua interpreter
       * @param ptr: pointer to the class instance
       * @param adopt : true if you adopt the object ownership to lua
       */
      template <typename T>
        object(lua_State *L, T *ptr, bool adopt = false);


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
        this->push();
        std::string str = luaL_tolstring(L, -1, NULL);
        lua_pop(L, 2);
        return str.c_str();
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

      virtual bool is_valid() const
      {
        if (! L) { return false; }
        if (ref == LUA_REFNIL) { return false; }
        return true;
      }


      /// Push the referred lua object onto the lua stack
      /**
       * @param L : lua interpreter
       */
      void push(lua_State *L) const
      {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
      }
      /// Push the referred lua object onto the lua stack
      /**
       * (within the same lua interpreter of this object)
       */
      void push() const
      {
        lua_rawgeti(this->L, LUA_REGISTRYINDEX, ref);
      }


      bool setmetatable(const object &t)
      {
        this->push();
        t.push();
        lua_setmetatable(L, -2);
        lua_pop(L, 1);
        return true;
      }


      /// makes a table
      /**
       * (supposing this object is type of table)
       * makes a table as a member object specified by the key.
       * when a table already exists as a member, then returns it.
       * @param key : index of this table object
       * @param force : true - if non-table member already exists,
       *                       overwrites the member with new table
       * @return the table object as a member
       */
      /// retrieves the talble object
      template <typename T>
        object table(const T &key, bool force = false);


      std::string tostring() const
      {
        return object_cast<std::string>(*this);
      }


      /// Type of refered lua object
      /**
       * @return object type (LUA_TXXX from lua.h)
       */
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
        class proxy operator[](const T &key) const;

      // lua function call
      object operator()();
      template <typename T1>
        object operator()(T1 arg1);
      template <typename T1, typename T2>
        object operator()(T1 arg1, T2 arg2);
      template <typename T1, typename T2, typename T3>
        object operator()(T1 arg1, T2 arg2, T3 arg3);

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

    protected:
      lua_State *L;
      int ref;
      friend class proxy;
  };


  class proxy
  {

    public:

      // Ctor (proxy binding)
      template <typename T>
        proxy(lua_State *L, int ref_srctable, const T &key);

//      proxy(lua_State *L, int ref_table, int ref_key)
//        : L(L), ref_table(ref_table), ref_key(ref_key)
//      {
////printf("REGIST (PROXY TABLE): %d\n", ref_table);
////printf("REGIST (PROXY KEY): %d\n", ref_key);
//      }


      // Ctor (copying)
      proxy(const proxy &src)
        : L(src.L)
      {
//printf("COPYING PROXY!\n");
        lua_rawgeti(L, LUA_REGISTRYINDEX, src.ref_table);
        ref_table = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF (TABLE): %d\n", ref_table);
        lua_rawgeti(L, LUA_REGISTRYINDEX, src.ref_key);
        ref_key = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF (KEY): %d\n", ref_key);
      }


      virtual ~proxy()
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

      bool push()
      {
        object(*this).push();
        return true;
      }

      int type() const
      {
        return object(*this).type();
      }

      // assignment
      template <typename T>
        void operator=(const T &val);

      // more index access
      template <typename T>
        proxy operator[](const T &key) const;

      object operator()() const
      {
        return object(*this)();
      }
      template <typename T1> const
        object operator()(T1 arg1)
      {
        return object(*this)(arg1);
      }
      template <typename T1, typename T2> const
        object operator()(T1 arg1, T2 arg2)
      {
        return object(*this)(arg1, arg2);
      }
      template <typename T1, typename T2, typename T3> const
        object operator()(T1 arg1, T2 arg2, T3 arg3)
      {
        return object(*this)(arg1, arg2, arg3);
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

  template <typename T>
    class reference: public object
  {
    public:
      reference()
        : p(NULL), object()
      { }

      reference(lua_State *L)
        : p(NULL), object(L)
      { }

      reference(lua_State *L, T *p, bool adopt = false)
        : object(L, p, adopt), p(p)
      { }

      reference(const from_stack &s)
        : object(), p(NULL)
      {
        reset(s);
      }

      reference(const object &src)
        : object(), p(NULL)
      {
        reset(src);
      }

      template <typename From>
        reference(const reference<From> &src)
        : object(src), p(src.get())
      {
      }


      template <typename From>
        reference(const class weak_ref<From> &src);


      ~reference()
      {
        p = NULL;
      }


      T *get() const
      {
        return p;
      }


      virtual bool is_valid() const
      {
printf("IS VALID?\n");
        return p != NULL;
      }


      bool reset(lua_State *L, T *p, bool adopt = false)
      {
printf("REF FROM PTER!\n");
        object::operator=(object(L, p, adopt));
        this->p = p;
        return true;
      }
      bool reset(const object &src)
      {
printf("REF FROM OBJECT!\n");
        try {
          p = object_cast<T *>(src);
          object::operator=(src);
        }
        catch (...) {
          p = NULL;
        }
        return true;
      }
      template <typename From>
        bool reset(const reference<From> &src)
      {
printf("REF FROM REF!\n");
        object::operator=(src);
        p = src.get();
        return true;
      }
      bool reset()
      {
        object::clear();
        p = NULL;
        return true;
      }


      long use_count() const
      {
        object ref = registry(L)["luaport"]["references"];
        object count = ref[lightuserdata(L, p)];
        return object_cast<long>(count);
      }


      template <typename From>
        void operator=(const reference<From> &src)
      {
printf("COPYING FROM REFERENCE!\n");
        reset(src);
      }
      void operator=(const object &src)
      {
        reset(src);
      }


      T* operator->() const
      {
        return p;
      }

      T& operator*() const
      {
        return *p;
      }

    protected:
      T *p;

    friend class weak_ref<T>;
  };


  template <typename T>
    class weak_ref
  {
    public:

      weak_ref() : u(NULL)
      {
      }

      weak_ref(const reference<T> &ref)
      {
        reset(ref);
      }

      ~weak_ref()
      { }

      reference<T> lock() const
      {
        if (! u) { return reference<T>(); }

        object ref = registry(u->L)["luaport"]["references"];
        if (ref[lightuserdata(u->L, u->p)])
        {
          return reference<T>(u->L, u->p, u->adopt);
        }
        return reference<T>();
      }

      bool reset(const reference<T> &ref)
      {
        ref.push();
        u = (detail::managed<T> *)lua_touserdata(ref.L, -1);
        lua_pop(ref.L, 1);
        return true;
      }


      weak_ref<T>& operator=(const reference<T> &ref)
      {
        reset(ref);
        return *this;
      }


    protected:
      detail::managed<T> *u;
  };


  // ---------------------------------------------------------
  // detail function declaration

  namespace detail
  {

    template <typename From, typename To>
      static void* downcast(void *from);

    static int lua_class_get_member(lua_State *L);
    static int lua_class_set_member(lua_State *L);
    inline static std::string lua_get_args_string(lua_State *L);

    // push to the stack
    static void push(lua_State *L, const bool &value);
    static void push(lua_State *L, const char *val);
    static void push(lua_State *L, const double &value);
    static void push(lua_State *L, const int &value);
    static void push(lua_State *L, const long &value);
    static void push(lua_State *L, const unsigned long &value);
    static void push(lua_State *L, const lua_CFunction &value);
    static void push(lua_State *L, const std::string &value);
    static void push(lua_State *L, const object &value);
    static void push(lua_State *L, const proxy &value);
    template <typename T>
      static void push(lua_State *L, T *val, bool adopt);
  //  template <typename T>
  //    static void push(lua_State *L, T *val, bool adopt = false);

  } // namespace detail


  // ---------------------------------------------------------
  // detail class defintion

  namespace detail
  {
    /// @cond DETAIL

    template <typename T, T arg>
      struct call_traits;

    template <typename T>
      struct cast_traits;

    template <typename T, T arg>
      struct cfunc_traits;

    template <typename T>
      struct finalizer
    {
      static int lfunc(lua_State *L);
    };
    template <typename T>
      struct finalizer<T *>
    {
      static int lfunc(lua_State *L);
    };

    template <typename T>
      struct functype_hold
    {
      template <T func>
        lua_CFunction get_lfunc()
      {
        return cfunc_traits<T, func>::lfunc;
      }
    };

    template <typename T>
      class managed
    {
      public:
        managed(lua_State *L, T *p, bool adopt)
          : L(L), p(p), adopt(adopt) { }
        virtual ~managed();

        // placement new
        static void* operator new(std::size_t, lua_State *L);

        T *p;
        lua_State *L;
        bool adopt;
    };

    template <typename T>
      struct type_traits
    {
      typedef T natural;
      static std::string name(lua_State *L);
    };
    template <typename T>
      struct type_traits<T &>
    {
      typedef T natural;
      static std::string name(lua_State *L);
    };
    template <typename T>
      struct type_traits<const T>
    {
      typedef T natural;
      static std::string name(lua_State *L);
    };
    template <typename T>
      struct type_traits<const T&>
    {
      typedef T natural;
      static std::string name(lua_State *L);
    };

    /// @endcond DETAIL
  }

} // namespace luaport


// ------------------------------------------------------
// inner function implementation
namespace luaport
{
  namespace detail
  {

    template <typename From, typename To>
      inline static void* downcast(void *from)
    {
      From *f = (From *)from;
  //printf("CAST FROM: %p\n", f);
      To *to = f;
  //printf("CAST TO: %p\n", to);
      return to;
    }


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

    inline static int lua_class_create(lua_State *L)
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
      m["__gc"] = finalizer<void>::lfunc;
      m["__index"] = lua_class_get_member;
      m["__newindex"] = lua_class_set_member;
      m.push();
      lua_setmetatable(L, -2);

      object u = from_stack(L, -1);
      printf("U: %s\n", (const char *)u);
      init(u);
      return 1;
    }


    inline static void lua_error_signature
      (lua_State *L, const std::string &sig)
    {
      std::string msg;
      msg += "signature: " + sig + "\n";
      msg += "but got: " + lua_get_args_string(L);
      luaL_error(L, "%s", msg.c_str());
    }


    inline static std::string lua_get_args_string(lua_State *L)
    {
      int nargs = lua_gettop(L);
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


    inline static int lua_class_get_member(lua_State *L)
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


    inline static int lua_class_set_member(lua_State *L)
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

  } // namespace detail

} // namespace luaport

// push function implementation
namespace luaport
{
  using namespace detail;

  namespace detail
  {
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
    inline void push(lua_State *L, const long &val)
    {
      lua_pushinteger(L, val);
    }
    inline void push(lua_State *L, const unsigned long &val)
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
    // avoiding from the compiler confusing
    template <>
      inline void push(lua_State *L, lua_CFunction val, bool adopt)
    {
      lua_pushcfunction(L, val);
    }
    template <typename T>
      inline void push(lua_State *L, T *val, bool adopt)
    {
  printf("PUSH UDATA: %p\n", val);
      managed<T> *u = new(L) managed<T>(L, val, adopt);
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
      m["__gc"] = finalizer<managed<T>*>::lfunc;
      m["__index"] = lua_class_get_member;
      m["__newindex"] = lua_class_set_member;
      m.push();
      lua_setmetatable(L, -2);

      printf("REGISTER REFERENCE: %p\n", val);
      object ref = registry(L)["luaport"]["references"];
      object count = ref[lightuserdata(L, val)];
      if (adopt)
      {
        if (count.type() == LUA_TNUMBER)
        {
          int c = object_cast<int>(count);
          printf("COUNT (BEFORE): %d\n", c);
          c++;
          ref[lightuserdata(L, val)] = c;
          printf("COUNT (AFTER): %d\n", c);
        }
        else
        {
          printf("COUNT (BEFORE): NIL\n");
          ref[lightuserdata(L, val)] = 1;
          printf("COUNT (AFTER): 1\n");
        }
      }
      else
      {
        if (! ref[lightuserdata(L, val)])
        {
          ref[lightuserdata(L, val)] = 0;
        }
      }
  //printf("TYPE: %s\n", lua_typename(L, lua_type(L, -1)));
    }

  } // namespace detail

} // namespace luaport


// -----------------------------------------------------
// inner class implementation

namespace luaport
{
  // call_traits struct implementation
  namespace detail
  {
    /// @cond DETAIL

    // call_traits 0
    template <typename R, R (*f)(lua_State*)>
      struct call_traits<R (*)(lua_State*), f>
    {
      static int call(lua_State *L)
      {
        luaport::push(L, (*f)(L)) ;
        return 1;
      }
    };
    template <void (*f)(lua_State*)>
      struct call_traits<void (*)(lua_State*), f>
    {
      static int call(lua_State *L)
      {
        (*f)(L);
        return 0;
      }
    };
    // call_traits 1
    template <typename R, typename T1, R (*f)(lua_State*,T1)>
      struct call_traits<R (*)(lua_State*,T1), f>
    {
      static int call(lua_State *L, T1 a1)
      {
        luaport::push(L, (*f)(L, a1) );
        return 1;
      }
    };
    template <typename T1, void (*f)(lua_State*,T1)>
      struct call_traits<void (*)(lua_State*,T1), f>
    {
      static int call(lua_State *L, T1 a1)
      {
        (*f)(L, a1);
        return 0;
      }
    };
    // call_traits 2
    template <typename R, typename T1,
              typename T2, R (*f)(lua_State*, T1, T2)>
      struct call_traits<R (*)(lua_State*, T1, T2), f>
    {
      static int call(lua_State *L, T1 a1, T2 a2)
      {
        luaport::push(L, (*f)(L, a1, a2) );
        return 1;
      }
    };
    template <typename T1, typename T2,
              void (*f)(lua_State*, T1, T2)>
      struct call_traits<void (*)(lua_State*, T1, T2), f>
    {
      static int call(lua_State *L, T1 a1, T2 a2)
      {
        (*f)(L, a1, a2);
        return 0;
      }
    };
    // call_traits 3
    template <typename R, typename T1, typename T2, typename T3,
              R (*f)(lua_State*, T1, T2, T3)>
      struct call_traits<R (*)(lua_State*, T1, T2, T3), f>
    {
      static int call(lua_State *L, T1 a1, T2 a2, T3 a3)
      {
        luaport::push(L, (*f)(L, a1, a2, a3) );
        return 1;
      }
    };
    template <typename T1, typename T2, typename T3,
              void (*f)(lua_State*, T1, T2, T3)>
      struct call_traits<void (*)(lua_State*, T1, T2, T3), f>
    {
      static int call(lua_State *L, T1 a1, T2 a2, T3 a3)
      {
        (*f)(L, a1, a2, a3);
        return 0;
      }
    };
    // call_traits 4
    template <typename R, typename T1, typename T2, typename T3, typename T4,
              R (*f)(lua_State*, T1, T2, T3, T4)>
      struct call_traits<R (*)(lua_State*, T1, T2, T3, T4), f>
    {
      static int call(lua_State *L, T1 a1, T2 a2, T3 a3, T4 a4)
      {
        luaport::push(L, (*f)(L, a1, a2, a3, a4) );
        return 1;
      }
    };
    template <typename T1, typename T2, typename T3, typename T4,
              void (*f)(lua_State*, T1, T2, T3, T4)>
      struct call_traits<void (*)(lua_State*, T1, T2, T3, T4), f>
    {
      static int call(lua_State *L, T1 a1, T2 a2, T3 a3, T4 a4)
      {
        (*f)(L, a1, a2, a3, a4);
        return 0;
      }
    };
    // call_traits 5
    template <typename R, typename T1, typename T2, typename T3,
              typename T4, typename T5,
              R (*f)(lua_State*, T1, T2, T3, T4, T5)>
      struct call_traits<R (*)(lua_State*, T1, T2, T3, T4, T5), f>
    {
      static int call(lua_State *L, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
      {
        luaport::push(L, (*f)(L, a1, a2, a3, a4, a5) );
        return 1;
      }
    };
    template <typename T1, typename T2, typename T3, typename T4,
              typename T5, void (*f)(lua_State*, T1, T2, T3, T4, T5)>
      struct call_traits<void (*)(lua_State*, T1, T2, T3, T4, T5), f>
    {
      static int call(lua_State *L, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
      {
        (*f)(L, a1, a2, a3, a4, a5);
        return 0;
      }
    };
    // call_traits 6
    template <typename R, typename T1, typename T2, typename T3,
              typename T4, typename T5, typename T6,
              R (*f)(lua_State*, T1, T2, T3, T4, T5, T6)>
      struct call_traits<R (*)(lua_State*, T1, T2, T3, T4, T5, T6), f>
    {
      static int call(lua_State *L, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6)
      {
        luaport::push(L, (*f)(L, a1, a2, a3, a4, a5, a6) );
        return 1;
      }
    };
    template <typename T1, typename T2, typename T3, typename T4,
              typename T5, typename T6,
              void (*f)(lua_State*, T1, T2, T3, T4, T5, T6)>
      struct call_traits<void (*)(lua_State*, T1, T2, T3, T4, T5, T6), f>
    {
      static int call(lua_State *L, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6)
      {
        (*f)(L, a1, a2, a3, a4, a5, a6);
        return 0;
      }
    };
    // call_traits 7
    template <typename R, typename T1, typename T2, typename T3,
              typename T4, typename T5, typename T6, typename T7,
              R (*f)(lua_State*, T1, T2, T3, T4, T5, T6, T7)>
      struct call_traits<R (*)(lua_State*, T1, T2, T3, T4, T5, T6, T7), f>
    {
      static int call(lua_State *L, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7)
      {
        luaport::push(L, (*f)(L, a1, a2, a3, a4, a5, a6, a7) );
        return 1;
      }
    };
    template <typename T1, typename T2, typename T3, typename T4,
              typename T5, typename T6, typename T7,
              void (*f)(lua_State*, T1, T2, T3, T4, T5, T6)>
      struct call_traits<void (*)(lua_State*, T1, T2, T3, T4, T5, T6, T7), f>
    {
      static int call(lua_State *L, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7)
      {
        (*f)(L, a1, a2, a3, a4, a5, a6, a7);
        return 0;
      }
    };

    /// @endcond DETAIL
  } // namespace detail

  // cfunc_traits struct implementation
  namespace detail
  {
    /// @cond DETAIL

    // cfunc_traits 0
    template <typename R, R (*f)(lua_State*)>
      struct cfunc_traits<R (*)(lua_State*), f>
    {
      static std::string sign(lua_State *L)
      {
        return get_typename<R>(L) + " ()";
      }
      static int lfunc(lua_State *L)
      {
        try {
          return call_traits<R (*)(lua_State*), f>::call(L);
        }
        catch (...) {
          lua_error_signature(L, sign(L) );
        }
        return 0;
      }
    };
    template <typename R, R (*f)()>
      struct cfunc_traits<R (*)(), f>
    {
      typedef cfunc_traits<R (*)(),f> thisclass;
      static R wrap(lua_State *L)
      {
        return f();
      }
      static int lfunc(lua_State *L)
      {
        cfunc_traits<R (*)(lua_State*), thisclass::wrap>::lfunc(L);
      }
    };
    template <typename R, typename C, R (C::*m)()>
      struct cfunc_traits<R (C::*)(), m>
    {
      typedef cfunc_traits<R (C::*)(),m> thisclass;
      static R flatten(C *c)
      {
        return (c->*m)();
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*), thisclass::flatten>::lfunc(L);
      }
    };
    template <typename R, typename C, R (C::*m)() const>
      struct cfunc_traits<R (C::*)() const, m>
    {
      typedef cfunc_traits<R (C::*)() const,m> thisclass;
      static R flatten(C *c)
      {
        return (c->*m)();
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*), thisclass::flatten>::lfunc(L);
      }
    };
    // cfunc_traits 1
    template <typename R, typename T1, R (*f)(lua_State*,T1)>
      struct cfunc_traits<R (*)(lua_State*,T1), f>
    {
      static std::string sign(lua_State *L)
      {
        return get_typename<R>(L) +
               " (" + get_typename<T1>(L) + ")";
      }
      static int lfunc(lua_State *L)
      {
        try {
          typename type_traits<T1>::natural arg1 =
            object_cast<typename type_traits<T1>::natural>(from_stack(L, 1));
          return call_traits<R (*)(lua_State*,T1), f>::call(L, arg1);
        }
        catch (...) {
          lua_error_signature(L, sign(L) );
        }
        return 0;
      }
    };
    template <typename R, typename T1, R (*f)(T1)>
      struct cfunc_traits<R (*)(T1), f>
    {
      typedef cfunc_traits<R (*)(T1),f> thisclass;
      static R wrap(lua_State *L, T1 a1)
      {
        return f(a1);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(lua_State*,T1), thisclass::wrap>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, R (C::*m)(T1)>
      struct cfunc_traits<R (C::*)(T1), m>
    {
      typedef cfunc_traits<R (C::*)(T1), m> thisclass;
      static R flatten(C *c, T1 a1)
      {
        return (c->*m)(a1);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1), thisclass::flatten>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, R (C::*m)(T1) const>
      struct cfunc_traits<R (C::*)(T1) const, m>
    {
      typedef cfunc_traits<R (C::*)(T1) const, m> thisclass;
      static R flatten(C *c, T1 a1)
      {
        return (c->*m)(a1);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1), thisclass::flatten>::lfunc(L);
      }
    };
    // cfunc_traits 2
    template <typename R, typename T1, typename T2,
              R (*f)(lua_State*, T1, T2)>
      struct cfunc_traits<R (*)(lua_State*, T1, T2), f>
    {
      static std::string sign(lua_State *L)
      {
        return get_typename<R>(L) +
               " (" + get_typename<T1>(L) +
               ", " + get_typename<T2>(L) + ")";
      }
      static int lfunc(lua_State *L)
      {
        try {
          typename type_traits<T1>::natural arg1 =
            object_cast<typename type_traits<T1>::natural>(from_stack(L, 1));
          typename type_traits<T2>::natural arg2 =
            object_cast<typename type_traits<T2>::natural>(from_stack(L, 2));
          return call_traits<R (*)(lua_State*,T1, T2), f>::call(L, arg1, arg2);
        }
        catch (...) {
          lua_error_signature(L, sign(L) );
        }
        return 0;
      }
    };
    template <typename R, typename T1, typename T2, R (*f)(T1, T2)>
      struct cfunc_traits<R (*)(T1, T2), f>
    {
      typedef cfunc_traits<R (*)(T1,T2),f> thisclass;
      static R wrap(lua_State *L, T1 a1, T2 a2)
      {
        return f(a1, a2);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(lua_State*,T1,T2), thisclass::wrap>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, typename T2,
              R (C::*m)(T1, T2)>
      struct cfunc_traits<R (C::*)(T1, T2), m>
    {
      typedef cfunc_traits<R (C::*)(T1,T2),m> thisclass;
      static R flatten(C *c, T1 a1, T2 a2)
      {
        return (c->*m)(a1, a2);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1,T2), thisclass::flatten>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, typename T2,
              R (C::*m)(T1, T2) const>
      struct cfunc_traits<R (C::*)(T1, T2) const, m>
    {
      typedef cfunc_traits<R (C::*)(T1,T2) const,m> thisclass;
      static R flatten(C *c, T1 a1, T2 a2)
      {
        return (c->*m)(a1, a2);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1,T2), thisclass::flatten>::lfunc(L);
      }
    };
    // cfunc_traits 3
    template <typename R, typename T1, typename T2, typename T3,
              R (*f)(lua_State*, T1, T2, T3)>
      struct cfunc_traits<R (*)(lua_State*, T1, T2, T3), f>
    {
      typedef R (*ftype)(lua_State*,T1,T2,T3);
      static std::string sign(lua_State *L)
      {
       return get_typename<R>(L) +
              " (" + get_typename<T1>(L) +
              ", " + get_typename<T2>(L) +
              ", " + get_typename<T3>(L) + ")";
      }
      static int lfunc(lua_State *L)
      {
        try {
          typename type_traits<T1>::natural arg1 =
            object_cast<typename type_traits<T1>::natural>(from_stack(L, 1));
          typename type_traits<T2>::natural arg2 =
            object_cast<typename type_traits<T2>::natural>(from_stack(L, 2));
          typename type_traits<T3>::natural arg3 =
            object_cast<typename type_traits<T3>::natural>(from_stack(L, 3));
          return call_traits<ftype, f>::call(L, arg1, arg2, arg3);
        }
        catch (...) {
          lua_error_signature(L, sign(L) );
        }
        return 0;
      }
    };
    template <typename R, typename T1, typename T2, typename T3, R (*f)(T1, T2, T3)>
      struct cfunc_traits<R (*)(T1, T2, T3), f>
    {
      typedef cfunc_traits<R (*)(T1,T2,T3),f> thisclass;
      static R wrap(lua_State *L, T1 a1, T2 a2, T3 a3)
      {
        return f(a1, a2, a3);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(lua_State*,T1,T2,T3), thisclass::wrap>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, typename T2, typename T3,
              R (C::*m)(T1, T2, T3)>
      struct cfunc_traits<R (C::*)(T1, T2, T3), m>
    {
      typedef cfunc_traits<R (C::*)(T1,T2,T3),m> thisclass;
      static R flatten(C *c, T1 a1, T2 a2, T3 a3)
      {
        return (c->*m)(a1, a2, a3);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1,T2,T3), thisclass::flatten>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, typename T2, typename T3,
              R (C::*m)(T1, T2, T3) const>
      struct cfunc_traits<R (C::*)(T1, T2, T3) const, m>
    {
      typedef cfunc_traits<R (C::*)(T1,T2,T3) const,m> thisclass;
      static R flatten(C *c, T1 a1, T2 a2, T3 a3)
      {
        return (c->*m)(a1, a2, a3);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1,T2,T3), thisclass::flatten>::lfunc(L);
      }
    };
    // cfunc_traits 4
    template <typename R, typename T1, typename T2, typename T3,
              typename T4, R (*f)(lua_State*, T1, T2, T3, T4)>
      struct cfunc_traits<R (*)(lua_State*, T1, T2, T3, T4), f>
    {
      typedef R (*ftype)(lua_State*,T1,T2,T3,T4);
      static std::string sign(lua_State *L)
      {
        return get_typename<R>(L) +
               " (" + get_typename<T1>(L) +
               ", " + get_typename<T2>(L) +
               ", " + get_typename<T3>(L) +
               ", " + get_typename<T4>(L) + ")";
      }
      static int lfunc(lua_State *L)
      {
        try {
          typename type_traits<T1>::natural arg1 =
            object_cast<typename type_traits<T1>::natural>(from_stack(L, 1));
          typename type_traits<T2>::natural arg2 =
            object_cast<typename type_traits<T2>::natural>(from_stack(L, 2));
          typename type_traits<T3>::natural arg3 =
            object_cast<typename type_traits<T3>::natural>(from_stack(L, 3));
          typename type_traits<T4>::natural arg4 =
            object_cast<typename type_traits<T4>::natural>(from_stack(L, 4));
          return call_traits<ftype, f>::call(L, arg1, arg2, arg3, arg4);
        }
        catch (...) {
          lua_error_signature(L, sign(L));
        }
        return 0;
      }
    };
    template <typename R, typename T1, typename T2, typename T3,
              typename T4, R (*f)(T1, T2, T3, T4)>
      struct cfunc_traits<R (*)(T1, T2, T3, T4), f>
    {
      typedef cfunc_traits<R (*)(T1,T2,T3,T4),f> thisclass;
      static R wrap(lua_State *L, T1 a1, T2 a2, T3 a3, T4 a4)
      {
        return f(a1, a2, a3, a4);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(lua_State*,T1,T2,T3,T4), thisclass::wrap>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, typename T2, typename T3,
              typename T4, R (C::*m)(T1, T2, T3, T4)>
      struct cfunc_traits<R (C::*)(T1, T2, T3, T4), m>
    {
      typedef cfunc_traits<R (C::*)(T1,T2,T3,T4),m> thisclass;
      static R flatten(C *c, T1 a1, T2 a2, T3 a3, T4 a4)
      {
        return (c->*m)(a1, a2, a3, a4);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1,T2,T3,T4), thisclass::flatten>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, typename T2, typename T3,
              typename T4, R (C::*m)(T1, T2, T3, T4) const>
      struct cfunc_traits<R (C::*)(T1, T2, T3, T4) const, m>
    {
      typedef cfunc_traits<R (C::*)(T1,T2,T3,T4) const,m> thisclass;
      static R flatten(C *c, T1 a1, T2 a2, T3 a3, T4 a4)
      {
        return (c->*m)(a1, a2, a3, a4);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1,T2,T3,T4), thisclass::flatten>::lfunc(L);
      }
    };
    // cfunc_traits 5
    template <typename R, typename T1, typename T2, typename T3, typename T4,
              typename T5, R (*f)(lua_State*, T1, T2, T3, T4, T5)>
      struct cfunc_traits<R (*)(lua_State*, T1, T2, T3, T4, T5), f>
    {
      typedef R (*ftype)(lua_State*,T1,T2,T3,T4,T5);
      static std::string sign(lua_State *L)
      {
        return get_typename<R>(L) +
               " (" + get_typename<T1>(L) +
               ", " + get_typename<T2>(L) +
               ", " + get_typename<T3>(L) +
               ", " + get_typename<T4>(L) +
               ", " + get_typename<T5>(L) + ")";
      }
      static int lfunc(lua_State *L)
      {
        try {
          typename type_traits<T1>::natural arg1 =
            object_cast<typename type_traits<T1>::natural>(from_stack(L, 1));
          typename type_traits<T2>::natural arg2 =
            object_cast<typename type_traits<T2>::natural>(from_stack(L, 2));
          typename type_traits<T3>::natural arg3 =
            object_cast<typename type_traits<T3>::natural>(from_stack(L, 3));
          typename type_traits<T4>::natural arg4 =
            object_cast<typename type_traits<T4>::natural>(from_stack(L, 4));
          typename type_traits<T5>::natural arg5 =
            object_cast<typename type_traits<T5>::natural>(from_stack(L, 5));
          return call_traits<ftype, f>::call(L, arg1, arg2, arg3, arg4, arg5);
        }
        catch (...) {
          lua_error_signature(L, sign(L));
        }
        return 0;
      }
    };
    template <typename R, typename T1, typename T2, typename T3, typename T4,
              typename T5, R (*f)(T1, T2, T3, T4, T5)>
      struct cfunc_traits<R (*)(T1, T2, T3, T4, T5), f>
    {
      typedef cfunc_traits<R (*)(T1,T2,T3,T4,T5),f> thisclass;
      static R wrap(lua_State *L, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
      {
        return f(a1, a2, a3, a4, a5);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(lua_State*,T1,T2,T3,T4,T5), thisclass::wrap>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, typename T2, typename T3,
              typename T4, typename T5, R (C::*m)(T1, T2, T3, T4, T5)>
      struct cfunc_traits<R (C::*)(T1, T2, T3, T4, T5), m>
    {
      typedef cfunc_traits<R (C::*)(T1,T2,T3,T4,T5),m> thisclass;
      static R flatten(C *c, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
      {
        return (c->*m)(a1, a2, a3, a4, a5);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1,T2,T3,T4,T5), thisclass::flatten>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, typename T2, typename T3,
              typename T4, typename T5, R (C::*m)(T1, T2, T3, T4, T5) const>
      struct cfunc_traits<R (C::*)(T1, T2, T3, T4, T5) const, m>
    {
      typedef cfunc_traits<R (C::*)(T1,T2,T3,T4,T5) const,m> thisclass;
      static R flatten(C *c, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
      {
        return (c->*m)(a1, a2, a3, a4, a5);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1,T2,T3,T4,T5), thisclass::flatten>::lfunc(L);
      }
    };
    // cfunc_traits 6
    template <typename R, typename T1, typename T2, typename T3, typename T4,
              typename T5, typename T6,
              R (*f)(lua_State*, T1, T2, T3, T4, T5, T6)>
      struct cfunc_traits<R (*)(lua_State*, T1, T2, T3, T4, T5, T6), f>
    {
      typedef R (*ftype)(lua_State*,T1,T2,T3,T4,T5,T6);
      static std::string sign(lua_State *L)
      {
        return get_typename<R>(L) +
               " (" + get_typename<T1>(L) +
               ", " + get_typename<T2>(L) +
               ", " + get_typename<T3>(L) +
               ", " + get_typename<T4>(L) +
               ", " + get_typename<T5>(L) +
               ", " + get_typename<T6>(L) + ")";
      }
      static int lfunc(lua_State *L)
      {
        try {
          typename type_traits<T1>::natural a1 =
            object_cast<typename type_traits<T1>::natural>(from_stack(L, 1));
          typename type_traits<T2>::natural a2 =
            object_cast<typename type_traits<T2>::natural>(from_stack(L, 2));
          typename type_traits<T3>::natural a3 =
            object_cast<typename type_traits<T3>::natural>(from_stack(L, 3));
          typename type_traits<T4>::natural a4 =
            object_cast<typename type_traits<T4>::natural>(from_stack(L, 4));
          typename type_traits<T5>::natural a5 =
            object_cast<typename type_traits<T5>::natural>(from_stack(L, 5));
          typename type_traits<T6>::natural a6 =
            object_cast<typename type_traits<T6>::natural>(from_stack(L, 6));
          return call_traits<ftype, f>::call(L,a1,a2,a3,a4,a5,a6);
        }
        catch (...) {
          lua_error_signature(L, sign(L));
        }
        return 0;
      }
    };
    template <typename R, typename T1, typename T2, typename T3, typename T4,
              typename T5, typename T6, R (*f)(T1, T2, T3, T4, T5, T6)>
      struct cfunc_traits<R (*)(T1, T2, T3, T4, T5, T6), f>
    {
      typedef cfunc_traits<R (*)(T1,T2,T3,T4,T5,T6),f> thisclass;
      static R wrap(lua_State *L, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6)
      {
        return f(a1, a2, a3, a4, a5, a6);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(lua_State*,T1,T2,T3,T4,T5,T6), thisclass::wrap>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, typename T2, typename T3,
              typename T4, typename T5, typename T6,
              R (C::*m)(T1, T2, T3, T4, T5, T6)>
      struct cfunc_traits<R (C::*)(T1, T2, T3, T4, T5, T6), m>
    {
      typedef cfunc_traits<R (C::*)(T1,T2,T3,T4,T5,T6),m> thisclass;
      static R flatten(C *c, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6)
      {
        return (c->*m)(a1,a2,a3,a4,a5,a6);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1,T2,T3,T4,T5,T6), thisclass::flatten>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, typename T2, typename T3,
              typename T4, typename T5, typename T6,
              R (C::*m)(T1, T2, T3, T4, T5, T6) const>
      struct cfunc_traits<R (C::*)(T1, T2, T3, T4, T5, T6) const, m>
    {
      typedef cfunc_traits<R (C::*)(T1,T2,T3,T4,T5,T6) const,m> thisclass;
      static R flatten(C *c, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6)
      {
        return (c->*m)(a1,a2,a3,a4,a5,a6);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1,T2,T3,T4,T5,T6), thisclass::flatten>::lfunc(L);
      }
    };
    // cfunc_traits 7
    template <typename R, typename T1, typename T2, typename T3, typename T4,
              typename T5, typename T6, typename T7,
              R (*f)(lua_State*, T1, T2, T3, T4, T5, T6, T7)>
      struct cfunc_traits<R (*)(lua_State*, T1, T2, T3, T4, T5, T6, T7), f>
    {
      typedef R (*ftype)(lua_State*,T1,T2,T3,T4,T5,T6,T7);
      static std::string sign(lua_State *L)
      {
        return get_typename<R>(L) +
               " (" + get_typename<T1>(L) +
               ", " + get_typename<T2>(L) +
               ", " + get_typename<T3>(L) +
               ", " + get_typename<T4>(L) +
               ", " + get_typename<T5>(L) +
               ", " + get_typename<T6>(L) +
               ", " + get_typename<T7>(L) + ")";
      }
      static int lfunc(lua_State *L)
      {
        try {
          typename type_traits<T1>::natural a1 =
            object_cast<typename type_traits<T1>::natural>(from_stack(L, 1));
          typename type_traits<T2>::natural a2 =
            object_cast<typename type_traits<T2>::natural>(from_stack(L, 2));
          typename type_traits<T3>::natural a3 =
            object_cast<typename type_traits<T3>::natural>(from_stack(L, 3));
          typename type_traits<T4>::natural a4 =
            object_cast<typename type_traits<T4>::natural>(from_stack(L, 4));
          typename type_traits<T5>::natural a5 =
            object_cast<typename type_traits<T5>::natural>(from_stack(L, 5));
          typename type_traits<T6>::natural a6 =
            object_cast<typename type_traits<T6>::natural>(from_stack(L, 6));
          typename type_traits<T7>::natural a7 =
            object_cast<typename type_traits<T7>::natural>(from_stack(L, 6));
          return call_traits<ftype, f>::call(L,a1,a2,a3,a4,a5,a6,a7);
        }
        catch (...) {
          lua_error_signature(L, sign(L));
        }
        return 0;
      }
    };
    template <typename R, typename T1, typename T2, typename T3, typename T4,
              typename T5, typename T6, typename T7,
              R (*f)(T1, T2, T3, T4, T5, T6, T7)>
      struct cfunc_traits<R (*)(T1, T2, T3, T4, T5, T6, T7), f>
    {
      typedef cfunc_traits<R (*)(T1,T2,T3,T4,T5,T6,T7),f> thisclass;
      static R wrap(lua_State *L, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7)
      {
        return f(a1, a2, a3, a4, a5, a6, a7);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(lua_State*,T1,T2,T3,T4,T5,T6,T7), thisclass::wrap>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, typename T2, typename T3,
              typename T4, typename T5, typename T6, typename T7,
              R (C::*m)(T1, T2, T3, T4, T5, T6, T7)>
      struct cfunc_traits<R (C::*)(T1, T2, T3, T4, T5, T6, T7), m>
    {
      typedef cfunc_traits<R (C::*)(T1,T2,T3,T4,T5,T6,T7),m> thisclass;
      static R flatten(C *c, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7)
      {
        return (c->*m)(a1,a2,a3,a4,a5,a6,a7);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1,T2,T3,T4,T5,T6,T7), thisclass::flatten>::lfunc(L);
      }
    };
    template <typename R, typename C, typename T1, typename T2, typename T3,
              typename T4, typename T5, typename T6, typename T7,
              R (C::*m)(T1, T2, T3, T4, T5, T6, T7) const>
      struct cfunc_traits<R (C::*)(T1, T2, T3, T4, T5, T6, T7) const, m>
    {
      typedef cfunc_traits<R (C::*)(T1,T2,T3,T4,T5,T6,T7) const,m> thisclass;
      static R flatten(C *c, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7)
      {
        return (c->*m)(a1,a2,a3,a4,a5,a6,a7);
      }
      static int lfunc(lua_State *L)
      {
        return cfunc_traits<R (*)(C*,T1,T2,T3,T4,T5,T6,T7), thisclass::flatten>::lfunc(L);
      }
    };

    /// @endcond DETAIL
  } // namespace detail

  // cast_traits struct implementatioin
  namespace detail
  {
    /// @cond DETAIL

    template <typename T>
      struct cast_traits
    {
      static T cast(const object &obj)
      {
  printf("INSTANCE CAST\n");
        reference<T> r = obj;
        return *r;
      }
    };
    template <>
      struct cast_traits<bool>
    {
      static bool cast(const object &obj)
      {
        lua_State *L = obj.interpreter();
        obj.push();
        bool b = lua_toboolean(L, -1);
        lua_pop(L, 1);
        return b;
      }
    };
    template <>
      struct cast_traits<double>
    {
      static double cast(const object &obj)
      {
        lua_State *L = obj.interpreter();
        obj.push();
        double d = lua_tonumber(L, -1);
        lua_pop(L, 1);
        return d;
      }
    };
    template <>
      struct cast_traits<int>
    {
      static int cast(const object &obj)
      {
        lua_State *L = obj.interpreter();
        obj.push();
        int i = lua_tointeger(L, -1);
        lua_pop(L, 1);
        return i;
      }
    };
    template <>
      struct cast_traits<long>
    {
      static long cast(const object &obj)
      {
        lua_State *L = obj.interpreter();
        obj.push();
        long l = lua_tointeger(L, -1);
        lua_pop(L, 1);
        return l;
      }
    };
    template <>
      struct cast_traits<unsigned char>
    {
      static unsigned char cast(const object &obj)
      {
        lua_State *L = obj.interpreter();
        obj.push();
        unsigned char u = lua_tointeger(L, -1);
        lua_pop(L, 1);
        return u;
      }
    };
    template <>
      struct cast_traits<lua_CFunction>
    {
      static lua_CFunction cast(const object &obj)
      {
        lua_State *L = obj.interpreter();
        obj.push();
        lua_CFunction f = lua_tocfunction(L, -1);
        lua_pop(L, 1);
        return f;
      }
    };
    template <>
      struct cast_traits<std::string>
    {
      static std::string cast(const object &obj)
      {
        size_t len;
        lua_State *L = obj.interpreter();
        obj.push();
        const char *c_str = luaL_tolstring(L, -1, &len);
        std::string str;
        if (c_str)
        {
          str.assign(c_str, 0, len);
        }
        lua_pop(L, 2);
        return str;
      }
    };
    template <>
      struct cast_traits<luaport::object>
    {
      static luaport::object cast(const object &obj)
      {
        return obj;
      }
    };
    template <typename T>
      struct cast_traits<luaport::reference<T> >
    {
      static luaport::reference<T> cast(const object &obj)
      {
  printf("CAST TO REFERENCE\n");
        return obj;
      }
    };
    template <typename T>
      struct cast_traits<T *>
    {
      static T* cast(const object &obj)
      {
        lua_State *L = obj.interpreter();
        object m = obj.getmetatable();
        if (! m.is_table()) { throw std::bad_cast(); }
        if (! m["luaport"].is_valid()) { throw std::bad_cast(); }

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
          if (m.type() != LUA_TTABLE) { throw std::bad_cast(); }
          c = m["__index"];
          object(m["downcast"]).push();
          void*(*cast)(void *) = (void* (*)(void*))lua_touserdata(L, -1);
          lua_pop(L, 1);
          if (cast)
          {
            p = cast(p);
          }
          else
          {
          }
        }
        return (T *)p;
      }
    };

    /// @endcond DETAIL
  } // namespace detail

  // finalizer class implementation
  namespace detail
  {
    template <typename T>
      inline int finalizer<T>::lfunc(lua_State *L)
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
      inline int finalizer<T *>::lfunc(lua_State *L)
    {
      managed<T> *u = (managed<T>*)lua_touserdata(L, 1);
      object inst = from_stack(L, 1);
      object f = inst["__finalize"];
      if (f.type() == LUA_TFUNCTION)
      {
        f(inst);
      }
      // call only the dtor (not delete)
      u->~managed();
      // lua will release the memory
      return 0;
    }

  } // namespace detail

  // managed class implementation
  namespace detail
  {

    template <typename T>
      managed<T>::~managed()
    {
      printf("RELEASE UDATA!\n");
      if (adopt)
      {
        object ref = registry(L)["luaport"]["references"];
        object count = ref[lightuserdata(L, p)];
        if (! count)
        {
          printf("ERROR!\n");
        }
        int c = object_cast<int>(count);
        printf("COUNT DOWN (BEFORE): %d\n", c);
        c--;
        printf("COUNT DOWN (ATER): %d\n", c);
        ref[lightuserdata(L, p)] = c;
        if (c == 0)
        {
          ref[lightuserdata(L, p)] = object();
          printf("RELEASE THE INSTANCE\n");
          delete p;
        }
      }
    }


    // placement new
    template <typename T>
      void* managed<T>::operator new(std::size_t, lua_State *L)
    {
      return lua_newuserdata(L, sizeof(managed<T>));
    }

    template <typename T>
      inline std::string type_traits<T>::name(lua_State *L)
    {
      object fmap = registry(L)["luaport"]["func_to_name"];
      object str = fmap[finalizer<T>::lfunc];
      if (str) { return str.tostring(); }
      str = fmap[finalizer<managed<T>*>::lfunc];
      if (str) { return str.tostring(); }
      return "[unknown]";
    }
    template <typename T>
      inline std::string type_traits<T &>::name(lua_State *L)
    {
      return type_traits<T>::name(L) + "&";
    }
    template <typename T>
      inline std::string type_traits<const T>::name(lua_State *L)
    {
      return "const " + type_traits<T>::name(L);
    }
    template <typename T>
      inline std::string type_traits<const T &>::name(lua_State *L)
    {
      return "const " + type_traits<T>::name(L) + "&";
    }

  }

} // namespace luaport

// function implementation
namespace luaport
{
  using namespace detail;

  // ------------------------------------------------------
  // function implementation

  template <typename T>
    inline std::string get_typename(lua_State *L)
  {
    return type_traits<T>::name(L);
  }


  template <typename T>
    inline object get_class(lua_State *L)
  {
    object func_to_class = registry(L)["luaport"]["func_to_class"];
    return func_to_class[finalizer<managed<T>*>::lfunc];
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
      class_to_func[c] = finalizer<void>::lfunc;
      object class_to_name = registry(L)["luaport"]["class_to_name"];
      class_to_name[c] = name;
      object name_to_class = registry(L)["luaport"]["name_to_class"];
      name_to_class[name] = c;

      m["__call"] = lua_class_create;
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
      class_to_func[c] = finalizer<managed<T>*>::lfunc;
      object class_to_name = registry(L)["luaport"]["class_to_name"];
      class_to_name[c] = name;
      object func_to_class = registry(L)["luaport"]["func_to_class"];
      func_to_class[finalizer<managed<T>*>::lfunc] = c;
      object func_to_name = registry(L)["luaport"]["func_to_name"];
      func_to_name[finalizer<managed<T>*>::lfunc] = name;
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
    return newclass<Derived>(L, name);
    object b = get_class<Base>(L);
    if (! b)
    {
      std::string msg = "unregistered class: ";
      throw luaport::exception(msg + typeid(Base).name());
    }
    object d = newclass<Derived>(L, name);
printf("D RETURN\n");
    object m = d.getmetatable();
    m["__index"] = b;
    m["downcast"] = lightuserdata(L, downcast<Derived, Base>);
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
    return cast_traits<T>::cast(obj);
  }


  inline void open(lua_State *L)
  {
    object port = registry(L).table("luaport");
    object class_to_name = port.table("class_to_name");
    object class_to_func = port.table("class_to_func");
    object func_to_name = port.table("func_to_name");
    object func_to_class = port.table("func_to_class");
    object name_to_class = port.table("name_to_class");
    object references = port.table("references");

    func_to_name[finalizer<int>::lfunc] = "int";
    func_to_name[finalizer<float>::lfunc] = "float";
    func_to_name[finalizer<double>::lfunc] = "double";
    func_to_name[finalizer<long>::lfunc] = "long";
    func_to_name[finalizer<std::string>::lfunc] = "string";

    globals(L)["class"] = lua_newclass;
  }


  inline int type(const class object &obj)
  {
    return obj.type();
  }


  inline object registry(lua_State *L)
  {
    lua_pushnil(L);
    lua_copy(L, LUA_REGISTRYINDEX, -1);
    object r = from_stack(L, -1);
    lua_pop(L, 1);
    return r;
  }

}


// ----------------------------------------------------------------
// class implementation

// iterator class implementation
namespace luaport
{
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
//    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_table);
//    int t = luaL_ref(L, LUA_REGISTRYINDEX);
//    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_key);
//    int k = luaL_ref(L, LUA_REGISTRYINDEX);
//    return proxy(L, t, k);

    return proxy(L, ref_table, key());
  }

}

// object class implementation
namespace luaport
{

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


  template <typename T>
    inline object::object(lua_State *L, const T &val)
    : L(L), ref(LUA_REFNIL)
  {
    assert(L != NULL);

    luaport::push(L, val);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }


  inline object::object(lua_State *L, const char *str)
    : L(L)
  {
    assert(L != NULL);

    lua_pushstring(L, str);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }


  template <typename T>
    inline object::object(lua_State *L, T *ptr, bool adopt)
    : L(L), ref(LUA_REFNIL)
  {
    if (L)
    {
      luaport::push(L, ptr, adopt);
      ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
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
    inline object object::table(const T &key, bool force)
  {
    object m = (*this)[key];
    if (m.type() == LUA_TTABLE) { return m; }
    if (m.type() == LUA_TNIL)
    {
      m = newtable(L);
      (*this)[key] = m;
      return m;
    }
    // m is non-table member
    if (! force)
    {
      std::string tname = this->tostring();
      std::string kname = object(L, key).tostring();
      std::string msg =
        "error on object::table method : \"" + tname + "\"" +
        " already has non-table member with key \"" + kname + "\"";
      throw exception(msg);
    }
    m = newtable(L);
    (*this)[key] = newtable(L);
    return m;
  }


  template <typename T>
    inline proxy object::operator[](const T &key) const
  {
//    this->push();
//    int ref_table = luaL_ref(L, LUA_REGISTRYINDEX);
//    luaport::push(L, key);
//    int ref_key = luaL_ref(L, LUA_REGISTRYINDEX);
//    return proxy(L, ref_table, ref_key);

    return proxy(L, ref, key);
  }


  // lua function call
  inline object object::operator()()
  {
    this->push();
    lua_call(L, 0, 1);
    object result = from_stack(L, -1);
    lua_pop(L, 1);
    return result;
  }
  template <typename T1>
    inline object object::operator()(T1 arg1)
  {
    this->push();
    luaport::push(L, arg1);
    lua_call(L, 1, 1);
    object result = from_stack(L, -1);
    lua_pop(L, 1);
    return result;
  }
  template <typename T1, typename T2>
    inline object object::operator()(T1 arg1, T2 arg2)
  {
    this->push();
    luaport::push(L, arg1);
    luaport::push(L, arg2);
    lua_call(L, 2, 1);
    object result = from_stack(L, -1);
    lua_pop(L, 1);
    return result;
  }
  template <typename T1, typename T2, typename T3>
    inline object object::operator()(T1 arg1, T2 arg2, T3 arg3)
  {
    this->push();
    luaport::push(L, arg1);
    luaport::push(L, arg2);
    luaport::push(L, arg3);
    lua_call(L, 3, 1);
    object result = from_stack(L, -1);
    lua_pop(L, 1);
    return result;
  }

}

// proxy clas implementation
namespace luaport
{

  // Ctor (proxy binding)
  template <typename T>
    inline proxy::proxy(lua_State *L, int ref_srctable, const T &key)
    : L(L)
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_srctable);
    ref_table = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF (TABLE): %d\n", ref_table);
    luaport::push(L, key);
    ref_key = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF (KEY): %d\n", ref_key);
  }


  // assignment
  template <typename T>
    inline void proxy::operator=(const T &val)
  {
//    if (ref_table == LUA_REFNIL)
//    {
//      luaL_error(L, "attempt to index a nil value");
//      return;
//    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_table);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_key);
    luaport::push(L, val);
    lua_settable(L, -3);
    lua_pop(L, 1);
  }

  // more index access
  template <typename T>
    inline proxy proxy::operator[](const T &key) const
  {
//        if (ref_table == LUA_REFNIL)
//        {
//          luaL_error(L, "attempt to index a nil value");
//        }
//      lua_rawgeti(L, LUA_REGISTRYINDEX, ref_table);
//      lua_rawgeti(L, LUA_REGISTRYINDEX, ref_key);
//      lua_gettable(L, -2);
//    int newtable = luaL_ref(L, LUA_REGISTRYINDEX);
//printf("REF (TABLE): %d\n", newtable);
//    lua_pop(L, 1);
//    luaport::push(L, key);
//    int newkey = luaL_ref(L, LUA_REGISTRYINDEX);
////printf("REF (KEY): %d\n", newkey);
//     return proxy(L, newtable, newkey);

    object t(*this);
    return proxy(L, t.ref, key);
  }

}

// reference class implementation
namespace luaport
{

  template <typename T> template <typename From>
    reference<T>::reference(const class weak_ref<From> &src)
    : object(), p(NULL)
  {
    reset(src.lock());
  }

}


#endif // _LUAPORT_HPP
