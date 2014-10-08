#include "qtlua.h"

namespace PrefSDK
{
    QtLua::LuaFunction::LuaFunction(): _state(nullptr), __this(nullptr)
    {
        this->_storedfunc.RegistryIdx = LUA_REFNIL;
    }

    QtLua::LuaFunction::LuaFunction(lua_State *l, int idx, QObject *__this): _state(l), __this(__this)
    {
        this->_iscfunction = lua_iscfunction(l, idx);

        if(this->_iscfunction)
            this->_storedfunc.CFunction = lua_tocfunction(l, idx);
        else
        {
            lua_pushvalue(l, idx);
            this->_storedfunc.RegistryIdx = luaL_ref(l, LUA_REGISTRYINDEX);
        }
    }

    QtLua::LuaFunction::LuaFunction(const QtLua::LuaFunction &lf)
    {
        *this = lf;
    }

    QtLua::LuaFunction::~LuaFunction()
    {
        if(!this->_iscfunction && (this->_storedfunc.RegistryIdx != LUA_REFNIL))
            luaL_unref(this->_state, LUA_REGISTRYINDEX, this->_storedfunc.RegistryIdx);

        this->_storedfunc.RegistryIdx = LUA_REFNIL;
        this->_state = nullptr;
        this->__this = nullptr;
    }

    void QtLua::LuaFunction::push() const
    {
        if(this->_iscfunction)
            lua_pushcfunction(this->_state, this->_storedfunc.CFunction);
        else if(this->_storedfunc.RegistryIdx != LUA_REFNIL)
            lua_rawgeti(this->_state, LUA_REGISTRYINDEX, this->_storedfunc.RegistryIdx);
    }

    bool QtLua::LuaFunction::operator()(int nargs, int nresults, bool threaded) const
    {
        bool err = false;
        lua_State* l = (threaded ? lua_newthread(this->_state) : this->_state);

        if(threaded)
            lua_xmove(this->_state, l, nargs);

        if(this->__this) /* Push self, if any */
        {
            QtLua::pushObject(l, this->__this);
            lua_insert(l, -(nargs + 1));
            nargs++;
        }

        this->push();
        lua_insert(l, -(nargs + 1));

        if(threaded)
        {
            err = lua_resume(l, nargs) != 0;

            if(err)
                lua_xmove(l, this->_state, 1); /* Copy Error */

            lua_remove(this->_state, -(nresults + 1)); /* Pop thread from stack */
        }
        else
            err = lua_pcall(this->_state, nargs, nresults, 0) != 0;

        return err;
    }

    QtLua::LuaFunction &QtLua::QtLua::LuaFunction::operator=(const QtLua::QtLua::LuaFunction &lf)
    {
        this->_state = lf._state;
        this->_iscfunction = lf._iscfunction;
        this->__this = lf.__this;

        if(lf._iscfunction)
            this->_storedfunc.CFunction = lf._storedfunc.CFunction;
        else if(lf._storedfunc.RegistryIdx != LUA_REFNIL)
        {
            lf.push();
            this->_storedfunc.RegistryIdx = luaL_ref(lf._state, LUA_REGISTRYINDEX);
        }

        return *this;
    }

    bool QtLua::LuaFunction::isValid() const
    {
        return this->_storedfunc.RegistryIdx != LUA_REFNIL;
    }

    lua_State *QtLua::LuaFunction::state() const
    {
        return this->_state;
    }

    lua_State* QtLua::_state = nullptr;
    QVector<luaL_Reg> QtLua::_methods;

    QtLua::QtLua()
    {

    }

    void QtLua::pushObject(lua_State* l, QObject *obj, QtLua::ObjectOwnership ownership)
    {
        QObject** pqobject = reinterpret_cast<QObject**>(lua_newuserdata(l, sizeof(QObject*)));
        *pqobject = obj;

        QtLua::pushMetaTable(l, ownership);
        lua_setmetatable(l, -2);
    }

    void QtLua::pushEnum(lua_State *l, const QMetaEnum &metaenum)
    {
        lua_newtable(l);

        for(int i = 0; i < metaenum.keyCount(); i++)
        {
            const char* k = metaenum.key(i);
            int v = metaenum.keyToValue(k);

            lua_pushinteger(l, v);
            lua_setfield(l, -2, k);
        }
    }

    bool QtLua::isQObject(lua_State *l, int idx)
    {
        return lua_isuserdata(l, idx) && (QtLua::toQObject(l, idx) != nullptr);
    }

    QObject *QtLua::toQObject(lua_State *l, int idx)
    {
        return (*reinterpret_cast<QObject**>(lua_touserdata(l, idx)));
    }

    void QtLua::registerObjectOwnership(lua_State *l)
    {
        lua_newtable(l);

        lua_pushinteger(l, QtLua::CppOwnership);
        lua_setfield(l, -2, "CppOwnership");

        lua_pushinteger(l, QtLua::LuaOwnership);
        lua_setfield(l, -2, "LuaOwnership");

        lua_setfield(l, -2, "ObjectOwnership");
    }

    void QtLua::open(lua_State *l)
    {
        if(QtLua::_state)
            return;

        qRegisterMetaType<lua_Integer>("lua_Integer");

        QtLua::_state = l;
        QtLua::_methods.append( {nullptr, nullptr} );

        luaL_register(l, "qt", QtLua::_methods.cbegin());
        QtLua::registerObjectOwnership(l);
        QtLua::registerQml(l);
        lua_pop(l, 1); /* Pop Table */
    }

    bool QtLua::isMethod(const QMetaObject *metaobj, const QString &member, int& idx)
    {
        for(int i = 0; i < metaobj->methodCount(); i++)
        {
            QMetaMethod metamethod = metaobj->method(i);

            if(QString::compare(member, metamethod.name()) == 0)
            {
                idx = i;
                return true;
            }
        }

        idx = -1;
        return false;
    }

    bool QtLua::isProperty(const QMetaObject *metaobj, const QString &member, int& idx)
    {
        for(int i = 0; i < metaobj->propertyCount(); i++)
        {
            QMetaProperty metaproperty = metaobj->property(i);

            if(QString::compare(member, metaproperty.name()) == 0)
            {
                idx = i;
                return true;
            }
        }

        idx = -1;
        return false;
    }

    bool QtLua::checkMetaIndexOverride(lua_State *l, QObject* qobject, const QMetaObject* metaobj)
    {
        int residx;

        if(QtLua::isMethod(metaobj, "metaIndex", residx))
        {
            bool success = false;
            int res = 0, t = lua_type(l, 2);

            if((t == LUA_TSTRING) && (metaobj->indexOfMethod(metaobj->normalizedSignature("metaIndex(lua_State*, QString)") )!= -1))
                success = metaobj->invokeMethod(qobject, "metaIndex", Q_RETURN_ARG(int, res), Q_ARG(lua_State*, l), Q_ARG(QString, QString::fromUtf8(lua_tostring(l, 2))));
            else if((t == LUA_TNUMBER) && (metaobj->indexOfMethod(metaobj->normalizedSignature("metaIndex(lua_State*, lua_Integer)")) != -1))
                success = metaobj->invokeMethod(qobject, "metaIndex", Q_RETURN_ARG(int, res), Q_ARG(lua_State*, l), Q_ARG(lua_Integer, lua_tointeger(l, 2)));

            return success && res;
        }

        return false;
    }

    bool QtLua::checkMetaNewIndexOverride(lua_State *l, QObject *qobject, const QMetaObject *metaobj)
    {
        int residx;

        if(QtLua::isMethod(metaobj, "metaNewIndex", residx))
        {
            bool success = false, res = false;
            int t = lua_type(l, 2);

            if((t == LUA_TSTRING) && (metaobj->indexOfMethod(metaobj->normalizedSignature("metaNewIndex(lua_State*, QString)") ) != -1))
                success = metaobj->invokeMethod(qobject, "metaNewIndex", Q_RETURN_ARG(bool, res), Q_ARG(lua_State*, l), Q_ARG(QString, QString::fromUtf8(lua_tostring(l, 2))));
            else if((t == LUA_TNUMBER) && (metaobj->indexOfMethod(metaobj->normalizedSignature("metaNewIndex(lua_State*, lua_Integer)")) != -1))
                success = metaobj->invokeMethod(qobject, "metaNewIndex", Q_RETURN_ARG(bool, res), Q_ARG(lua_State*, l), Q_ARG(lua_Integer, lua_tointeger(l, 2)));

            return success && res;
        }

        return false;
    }

    void QtLua::pushMetaTable(lua_State *l, QtLua::ObjectOwnership ownership)
    {
        lua_newtable(l);
        lua_pushcfunction(l, &QtLua::metaIndex);
        lua_setfield(l, -2, "__index");
        lua_pushcfunction(l, &QtLua::metaNewIndex);
        lua_setfield(l, -2, "__newindex");

        if(ownership == QtLua::LuaOwnership)
        {
            lua_pushcfunction(l, &QtLua::metaGc);
            lua_setfield(l, -2, "__gc");
        }
    }

    QString QtLua::getNameField(lua_State *l, int idx)
    {
        QString s;
        lua_getfield(l, idx, "name");
        int t = lua_type(l, -1);

        if(t <= 0) /* lua_isnoneornil() */
            throw PrefException("qt.qml.load(): Missing 'name' field");
        else if(t != LUA_TSTRING)
            throw PrefException("qt.qml.load(): 'string' type expected for 'name' field");
        else
            s = QString::fromUtf8(lua_tostring(l, -1));

        lua_pop(l, 1);
        return s;
    }

    QObject *QtLua::getObjectField(lua_State *l, int idx)
    {
        QObject* obj = nullptr;
        lua_getfield(l, idx, "object");
        int t = lua_type(l, -1);

        if(t <= 0) /* lua_isnoneornil() */
            throw PrefException("qt.qml.load(): Missing 'object' field");
        else if(t != LUA_TUSERDATA)
            throw PrefException("qt.qml.load(): 'userdata' type expected for 'object' field");
        else
            obj = *(reinterpret_cast<QObject**>(lua_touserdata(l, -1)));

        lua_pop(l, 1);
        return obj;
    }

    int QtLua::qmlLoad(lua_State *l)
    {
        int argc = lua_gettop(l);

        if(!argc)
        {
            throw PrefException("qt.qml.load(): Expected at least 1 argument");
            return 0;
        }

        if(argc >= 1)
        {
            if(lua_type(l, 1) != LUA_TSTRING)
            {
                throw PrefException("qt.qml.load(): Argument 1 must be a string type");
                return 0;
            }

            if(argc > 1)
            {
                for(int i = 2; i <= argc; i++)
                {
                    if(lua_type(l, i) != LUA_TTABLE)
                    {
                        throw PrefException(QString("qt.qml.load(): Argument %d must be a 'table' type").arg(i));
                        return 0;
                    }
                }
            }
        }

        QString qmlmain = QString::fromUtf8(lua_tostring(l, 1));
        QQuickView* view = new QQuickView();
        QQmlContext* ctx = view->rootContext();
        //ctx->setBaseUrl(QFileInfo(qmlmain).absolutePath());

        for(int i = 2; i <= argc; i++)
        {
            QString name = QtLua::getNameField(l, i);
            QObject* object = QtLua::getObjectField(l, i);
            ctx->setContextProperty(name, object);
        }

        view->setResizeMode(QQuickView::SizeRootObjectToView);
        view->setSource(QUrl::fromLocalFile(qmlmain));
        QtLua::pushObject(l, QWidget::createWindowContainer(view));
        return 1;
    }

    void QtLua::registerQml(lua_State *l)
    {
        lua_newtable(l);

        lua_pushcfunction(l, &QtLua::qmlLoad);
        lua_setfield(l, -2, "load");

        lua_setfield(l, -2, "qml");
    }

    int QtLua::metaIndex(lua_State *l)
    {
        int residx = -1;
        QObject* qobject = *(reinterpret_cast<QObject**>(lua_touserdata(l, 1)));
        const QMetaObject* metaobj = qobject->metaObject();

        if(QtLua::checkMetaIndexOverride(l, qobject, metaobj))  /* Allow MetaTable override */
            return 1;

        if(lua_type(l, 2) != LUA_TSTRING)
        {
            throw PrefException(QString("QtLua::metaIndex(): Invalid Member Type for '%1'").arg(QString::fromUtf8(metaobj->className())));
            return 0;
        }

        QString member = QString::fromUtf8(lua_tostring(l, 2));

        if(QtLua::isMethod(metaobj, member, residx))
        {
            QMetaMethod metamethod = metaobj->method(residx);

            lua_pushinteger(l, residx);
            lua_pushstring(l, metamethod.name().constData());
            lua_pushcclosure(l, &QtLua::methodCall, 2);
        }
        else if(QtLua::isProperty(metaobj, member, residx))
        {
            QMetaProperty metaproperty = metaobj->property(residx);
            int proptype = metaproperty.userType();

            if(!metaproperty.isReadable())
            {
                throw PrefException(QString("QtLua::metaIndex(): Property '%1' is Write-Only for '%2'").arg(member, QString::fromUtf8(metaobj->className())));
                return 0;
            }

            QVariant v = metaproperty.read(qobject);

            if(v.userType() == QMetaType::Bool)
                lua_pushboolean(l, v.toBool());
            else if(v.userType() == QMetaType::type("lua_Integer"))
                lua_pushinteger(l, v.value<lua_Integer>());
            else if(v.userType() == QMetaType::Int)
                lua_pushinteger(l, v.toInt());
            else if(v.userType() == QMetaType::UInt)
                lua_pushinteger(l, v.toUInt());
            else if(v.userType() == QMetaType::Long)
                lua_pushinteger(l, v.value<long>());
            else if(v.userType() == QMetaType::LongLong)
                lua_pushinteger(l, v.toLongLong());
            else if(v.userType() == QMetaType::ULongLong)
                lua_pushinteger(l, v.toULongLong());
            else if(v.userType() == QMetaType::Double)
                lua_pushnumber(l, v.toDouble());
            else if(v.userType() == QMetaType::QString)
                lua_pushstring(l, v.toString().toUtf8().constData());
            else if(v.userType() == QMetaType::type("PrefSDK::QtLua::LuaFunction"))
                v.value<QtLua::LuaFunction>().push();
            else if(v.canConvert(QMetaType::QObjectStar))
                QtLua::pushObject(l, v.value<QObject*>());
            else
            {
                throw PrefException(QString("QtLua::metaIndex(): Unsupported Type: '%1' for '%2'").arg(QString::fromUtf8(QMetaType::typeName(proptype)), QString::fromUtf8(metaobj->className())));
                return 0;
            }
        }
        else
        {
            throw PrefException(QString("QtLua::metaIndex(): Invalid member '%1' for '%2'").arg(member, QString::fromUtf8(metaobj->className())));
            return 0;
        }

        return 1;
    }

    int QtLua::metaNewIndex(lua_State *l)
    {
        QObject* qobject = *(reinterpret_cast<QObject**>(lua_touserdata(l, 1)));
        const QMetaObject* metaobj = qobject->metaObject();

        if(QtLua::checkMetaNewIndexOverride(l, qobject, metaobj))  /* Allow MetaTable override */
            return 0;

        int propidx = -1;
        QString member = QString::fromUtf8(lua_tostring(l, 2));

        if(QtLua::isProperty(metaobj, member, propidx))
        {
            QMetaProperty metaproperty = metaobj->property(propidx);
            int proptype = metaproperty.userType();
            int luaproptype = lua_type(l, 3);

            if(!metaproperty.isWritable())
            {
                throw PrefException(QString("QtLua::metaIndex(): Property '%1' is Read-Only for '%2'").arg(member, QString::fromUtf8(metaobj->className())));
                return 0;
            }

            if(luaproptype == LUA_TBOOLEAN && proptype == QMetaType::Bool)
                metaproperty.write(qobject, QVariant((lua_toboolean(l, 3) == true)));
            else if(luaproptype == LUA_TNUMBER)
            {
                if(proptype == QMetaType::type("lua_Integer"))
                    metaproperty.write(qobject, QVariant::fromValue(lua_tointeger(l, 3)));
                else if((proptype == QMetaType::Int))
                    metaproperty.write(qobject, QVariant(static_cast<int>(lua_tointeger(l, 3))));
                else if(proptype == QMetaType::UInt)
                    metaproperty.write(qobject, QVariant(static_cast<uint>(lua_tointeger(l, 3))));
                else if(proptype == QMetaType::LongLong)
                    metaproperty.write(qobject, QVariant(static_cast<qlonglong>(lua_tointeger(l, 3))));
                else if(proptype == QMetaType::ULongLong)
                    metaproperty.write(qobject, QVariant(static_cast<qulonglong>(lua_tointeger(l, 3))));
                else if(proptype == QMetaType::Double)
                    metaproperty.write(qobject, QVariant(lua_tonumber(l, 3)));
                else
                    throw PrefException(QString("QtLua::metaNewIndex(): Unsupported Integer Type: '%1' for '%2'").arg(QString::fromUtf8(QMetaType::typeName(proptype)), QString::fromUtf8(metaobj->className())));
            }
            else if(luaproptype == LUA_TSTRING)
                metaproperty.write(qobject, QVariant(QString::fromUtf8(lua_tostring(l, 3))));
            else if(luaproptype == LUA_TFUNCTION)
                metaproperty.write(qobject, QVariant::fromValue(QtLua::LuaFunction(l, 3, qobject)));
            else
                throw PrefException(QString("QtLua::metaNewIndex(): Unsupported Lua Type: '%1' for '%2'").arg(QString::fromUtf8(lua_typename(l, luaproptype)), QString::fromUtf8(metaobj->className())));
        }
        else
            throw PrefException(QString("QtLua::metaNewIndex(): Invalid Property: '%1' for '%2'").arg(member, QString::fromUtf8(metaobj->className())));

        return 0;
    }

    int QtLua::metaGc(lua_State *l)
    {
        QObject* qobject = *(reinterpret_cast<QObject**>(lua_touserdata(l, 1)));
        qobject->~QObject(); /* Call Destructor */
        return 0;
    }

    int QtLua::methodCall(lua_State *l)
    {
        int argc = lua_gettop(l);
        QObject* qobject = *(reinterpret_cast<QObject**>(lua_touserdata(l, 1)));
        lua_Integer methodidx = lua_tointeger(l, lua_upvalueindex(1));
        const char* methodname = lua_tostring(l, lua_upvalueindex(2));

        const QMetaObject* metaobj = qobject->metaObject();
        QMetaMethod metamethod = metaobj->method(methodidx);

        QVector<QVariant> args(10, QVariant());

        /* function mt.__ctor(metaself, self, ...) */

        for(int i = 2; i <= argc; i++)
        {
            int type = lua_type(l, i);
            int n = i - 2;

            if(type == LUA_TBOOLEAN)
                args[n] = QVariant((lua_toboolean(l, i) == true));
            else if(type == LUA_TNUMBER)
                args[n] = QVariant::fromValue(static_cast<lua_Integer>(lua_tointeger(l, i)));
            else if(type == LUA_TSTRING)
                args[n] = QVariant(QString::fromUtf8(lua_tostring(l, i)));
            else if(type == LUA_TUSERDATA)
                args[n] = QVariant::fromValue(*(reinterpret_cast<QObject**>(lua_touserdata(l, i))));
            else if(type == LUA_TFUNCTION)
                args[n] = QVariant::fromValue(QtLua::LuaFunction(l, i, qobject));
            else
                throw PrefException(QString("QtLua::methodCall(): Unsupported Parameter Lua Type: '%1' for '%2'").arg(QString::fromUtf8(lua_typename(l, type)), QString::fromUtf8(methodname)));
        }

        int returntype = metamethod.returnType();

        if(returntype == QMetaType::UnknownType)
        {
            throw PrefException(QString("Invalid MetaType: '%1' (maybe you need to register it with qRegisterMetaType())").arg(QString::fromUtf8(metamethod.typeName())));
            return 0;
        }

        bool isretvoid = (returntype == QMetaType::Void);
        QVariant retvar = (!isretvoid ? QVariant(returntype, nullptr) : QVariant());
        void* retdata = retvar.data();

        bool success = QMetaObject::invokeMethod(qobject, methodname, (!isretvoid ? QGenericReturnArgument(metamethod.typeName(), ((retvar.canConvert(QMetaType::QObjectStar)) ? &retdata : retdata)) : QGenericReturnArgument()),
                                                 (args[0].isValid() ? QGenericArgument(args[0].typeName(), args[0].data()) : QGenericArgument(0)),
                                                 (args[1].isValid() ? QGenericArgument(args[1].typeName(), args[1].data()) : QGenericArgument(0)),
                                                 (args[2].isValid() ? QGenericArgument(args[2].typeName(), args[2].data()) : QGenericArgument(0)),
                                                 (args[3].isValid() ? QGenericArgument(args[3].typeName(), args[3].data()) : QGenericArgument(0)),
                                                 (args[4].isValid() ? QGenericArgument(args[4].typeName(), args[4].data()) : QGenericArgument(0)),
                                                 (args[5].isValid() ? QGenericArgument(args[5].typeName(), args[5].data()) : QGenericArgument(0)),
                                                 (args[6].isValid() ? QGenericArgument(args[6].typeName(), args[6].data()) : QGenericArgument(0)),
                                                 (args[7].isValid() ? QGenericArgument(args[7].typeName(), args[7].data()) : QGenericArgument(0)),
                                                 (args[8].isValid() ? QGenericArgument(args[8].typeName(), args[8].data()) : QGenericArgument(0)),
                                                 (args[9].isValid() ? QGenericArgument(args[9].typeName(), args[9].data()) : QGenericArgument(0)));

        if(!success)
        {
            throw PrefException(QString("QtLua::methodCall(): Failed to call '%1'").arg(QString::fromUtf8(methodname)));
            return 0;
        }

        if(isretvoid)
            return 0;

        if(retvar.canConvert(QMetaType::QObjectStar))
        {
            if(retdata)
                QtLua::pushObject(l, reinterpret_cast<QObject*>(retdata));
            else
                lua_pushnil(l); /* Support 'null' return value */
        }
        else if(returntype == QMetaType::Bool)
            lua_pushboolean(l, retvar.toBool());
        else if(returntype == QMetaType::Int)
            lua_pushinteger(l, retvar.toInt());
        else if(returntype == QMetaType::UInt)
            lua_pushinteger(l, retvar.toUInt());
        else if(returntype == QMetaType::LongLong)
            lua_pushinteger(l, retvar.toLongLong());
        else if(returntype == QMetaType::Double)
            lua_pushnumber(l, retvar.toDouble());
        else if(returntype == QMetaType::QString)
            lua_pushstring(l, retvar.toString().toUtf8().constData());
        else if(returntype == QMetaType::type("lua_Integer"))
            lua_pushinteger(l, retvar.value<lua_Integer>());
        else
            isretvoid = true; /* Unknown type, ignore it */

        return !isretvoid;
    }
}