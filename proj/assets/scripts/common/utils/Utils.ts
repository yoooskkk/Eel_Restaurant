/**
 * @description 公共工具
 */

const VIEW_ACTION_TAG = 999;

export class Utils implements ISingleton {
    static module: string = "【Utils】";
    module: string = null!;
    /**@description 显示视图动画 */
    showView(node: cc.Node | null, complete: Function) {
        if (node) {
            cc.Tween.stopAllByTag(VIEW_ACTION_TAG);
            cc.tween(node).tag(VIEW_ACTION_TAG)
                .set({ scale: 0.2 })
                .to(0.2, { scale: 1.15 })
                .delay(0.05)
                .to(0.1, { scale: 1 })
                .call(() => {
                    if (complete) complete();
                })
                .start();
        }
    }

    /**@description 隐藏/关闭视图统一动画 */
    hideView(node: cc.Node | null, complete: Function) {
        if (node) {
            cc.Tween.stopAllByTag(VIEW_ACTION_TAG);
            cc.tween(node).tag(VIEW_ACTION_TAG)
                .to(0.2, { scale: 1.15 })
                .to(0.1, { scale: 0.3 })
                .call(() => {
                    if (complete) complete();
                })
                .start();
        }
    }

    getRandomNumber(min: number, max: number, isInteger: boolean = true): number {
        const random = Math.random() * (max - min) + min;
        return isInteger ? Math.floor(random) : random;
    }
}