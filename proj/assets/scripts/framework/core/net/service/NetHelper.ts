import { Macro } from "../../../defines/Macros";

/**
 * @description NetHelper 网络辅助管理 对 Sender 及 Handler的管理
 */
export default class NetHelper implements ISingleton {
    destory(...args: any[]): void {
        this.destorySender();
        this.destoryHandler();
    }
    clear(...args: any[]): void {
        this.clearSender();
        this.clearHandler();
    }
    static module: string = "【网络辅助器】";
    module: string = null!;
    /**@description 所有Sender */
    private _senders: Map<string, Sender> = new Map();
    /**@description 所有Handler */
    private _handlers: Map<string, Handler> = new Map();

    /**
     * @description 获取Sender
     * @param ClassOrModule sender类型或模块名 
     * @param isCreate 传入类型时有效 如果不存在，则创建，默认为创建
     * @returns 
     */
    getSender<T extends Sender>(ClassOrModule: SenderClass<T> | string, isCreate = true): T | null {
        let module = this.getSenderModule(ClassOrModule);
        if (module == Macro.UNKNOWN) {
            return null;
        }
        if (this._senders.has(module)) {
            return <T>this._senders.get(module);
        }
        if (typeof ClassOrModule != "string") {
            if (isCreate) {
                let data = new ClassOrModule();
                data.module = ClassOrModule.module;
                data.onLoad();
                this._senders.set(data.module, data);
                return <T>data;
            }
        }
        return null;
    }

    /**
     * @description 销毁sender
     * @param ClassOrModule 
     * @returns 
     */
    destorySender<T extends Sender>(ClassOrModule?: SenderClass<T> | string) {
        if ( ClassOrModule ){
            let module = this.getSenderModule(ClassOrModule);
            let sender = this._senders.get(module);
            if (sender) {
                sender.onDestroy();
                this._senders.delete(module);
                return true;
            }
            return false;
        }else{
            //删除所有
            this._senders.forEach((value)=>{
                value.onDestroy();;
            })
            this._senders.clear();
            return true;
        }
    }

    /**
     * @description 清除Sender
     * @param exclude 需要排除Sender
     */
    clearSender<T extends Sender>(exclude?: (SenderClass<T> | string)[]) {
        this._senders.forEach((data, key) => {
            if (!this.isInExcludeSender(data, exclude)) {
                this.destorySender(key);
            }
        });
    }

    private isInExcludeSender<T extends Sender>(data: T, exclude?: (SenderClass<T> | string)[]) {
        if (!exclude) return false;
        for (let i = 0; i < exclude.length; i++) {
            let module = this.getSenderModule(exclude[i]);
            if (data.module == module) {
                return true;
            }
        }
        return false;
    }

    private getSenderModule<T extends Sender>(ClassOrModule: SenderClass<T> | string) {
        let module = Macro.UNKNOWN;
        if (typeof ClassOrModule == "string") {
            module = ClassOrModule;
        } else {
            module = ClassOrModule.module;
        }
        return module;
    }

    /**
     * @description 获取Handler
     * @param ClassOrModule Handler类型或模块名 
     * @param isCreate 传入类型时有效 如果不存在，则创建，默认为false
     * @returns 
     */
    getHandler<T extends Handler>(ClassOrModule: HandlerClass<T> | string, isCreate = false): T | null {
        let module = this.getHandlerModule(ClassOrModule);
        if (module == Macro.UNKNOWN) {
            return null;
        }
        if (this._handlers.has(module)) {
            return <T>this._handlers.get(module);
        }
        if (typeof ClassOrModule != "string") {
            if (isCreate) {
                let data = new ClassOrModule();
                data.module = ClassOrModule.module;
                data.onLoad();
                this._handlers.set(data.module, data);
                return <T>data;
            }
        }
        return null;
    }

    /**
     * @description 销毁Handler
     * @param ClassOrModule 
     * @returns 
     */
    destoryHandler<T extends Handler>(ClassOrModule?: HandlerClass<T> | string) {
        if (ClassOrModule){
            let module = this.getHandlerModule(ClassOrModule);
            let handler = this._handlers.get(module);
            if (handler) {
                handler.onDestroy();
                this._handlers.delete(module);
                return true;
            }
            return false;
        }else{
            this._handlers.forEach((value)=>{
                value.onDestroy();
            })
            this._handlers.clear();
            return true;
        }
        
    }

    /**
     * @description 清除Handler
     * @param exclude 需要排除Handler
     */
    clearHandler<T extends Handler>(exclude?: (HandlerClass<T> | string)[]) {
        this._handlers.forEach((data, key) => {
            if (!this.isInExcludeHandler(data, exclude)) {
                this.destoryHandler(key);
            }
        });
    }

    private isInExcludeHandler<T extends Handler>(data: T, exclude?: (HandlerClass<T> | string)[]) {
        if (!exclude) return false;
        for (let i = 0; i < exclude.length; i++) {
            let module = this.getHandlerModule(exclude[i]);
            if (data.module == module) {
                return true;
            }
        }
        return false;
    }

    private getHandlerModule<T extends Handler>(ClassOrModule: HandlerClass<T> | string) {
        let module = Macro.UNKNOWN;
        if (typeof ClassOrModule == "string") {
            module = ClassOrModule;
        } else {
            module = ClassOrModule.module;
        }
        return module;
    }

    debug(){
        Log.d(`-----------网络辅助相关信息------------`);
        Log.d(`-----------当前所有Sender------------`);
        this._senders.forEach((data) => {
            Log.d(data.module);
        });
        Log.d(`-----------当前所有Handler------------`);
        this._handlers.forEach((data) => {
            Log.d(data.module);
        });
    }

}