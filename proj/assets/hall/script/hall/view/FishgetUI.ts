import UIView from "../../../../scripts/framework/core/ui/UIView";


const { ccclass, property } = cc._decorator;

@ccclass
export default class FishgetUI extends UIView {

    static getPrefabUrl() {
        return `prefabs/FishgetUIView`;
    }

    backbtn: cc.Node;
    pushbtn: cc.Node;
    pullbtn: cc.Node;
    bagbtn: cc.Node;
    rog: cc.Node;
    line: cc.Node;
    fishshadow: cc.Node
    rodtip: cc.Node

    graphics: cc.Graphics

    @property(cc.Color) lineColor: cc.Color = cc.Color.BLACK; // 线的颜色

    drlineflag: boolean = true

    // LIFE-CYCLE CALLBACKS:

    onLoad() {
        super.onLoad()
        this.backbtn = cc.find('backbtn', this.node);
        this.pushbtn = cc.find('pushbtn', this.node);
        this.pullbtn = cc.find('pullbtn', this.node);
        this.bagbtn = cc.find('bagbtn', this.node);
        this.rog = cc.find('rog', this.node);
        this.line = cc.find('line', this.node);
        this.fishshadow = cc.find('fishshadow', this.node);
        this.rodtip = cc.find('rog/rodtip', this.node)

        this.rog.angle = 0;
        this.fishshadow.position = cc.v3(720, 270)

        this.backbtn.on(cc.Node.EventType.TOUCH_END, () => {
            this.onClose();
        }, this)
        this.pushbtn.on(cc.Node.EventType.TOUCH_END, () => {
            // this.onClose();
            this.pushRog()
        }, this)
        this.pullbtn.on(cc.Node.EventType.TOUCH_END, () => {
            // this.onClose();
        }, this)
        this.bagbtn.on(cc.Node.EventType.TOUCH_END, () => {
            // this.onClose();
        }, this)

        //绘制钓鱼线效果
        this.drlineflag = false;
        this.graphics = cc.find('drline', this.node).getComponent(cc.Graphics)
    }

    start() {

    }

    public onClose(): void {
        super.close()
    }

    public pushRog() {
        this.rog.angle = -90
        cc.Tween.stopAllByTarget(this.rog)
        cc.tween(this.rog)
            .to(0.2, { angle: 0 }, { easing: "backOut" })
            // .to(0.3, { angle: 0 }, { easing: "elasticOut" })
            .call(() => {
                this.drlineflag = true
            })
            .start()
        this.fishshadow.active = false
        cc.Tween.stopAllByTarget(this.fishshadow)
        cc.tween(this.fishshadow)
            .to(0.2, { position: cc.v3(Math.random() * 200, Math.random() * 20, 0) })
            .call(() => {
                this.fishshadow.active = true
            })
            .start()
    }

    drawFishingLine() {
        if (!this.rodtip || !this.fishshadow) return;

        this.graphics.clear(); // 清除上次绘制的线

        // 获取世界坐标
        let startWorld = this.rodtip.convertToWorldSpaceAR(cc.Vec2.ZERO);
        let endWorld = this.fishshadow.convertToWorldSpaceAR(cc.Vec2.ZERO);

        // 转换到当前 Graphics 节点的本地坐标
        let start = this.node.convertToNodeSpaceAR(startWorld);
        let end = this.node.convertToNodeSpaceAR(endWorld);

        // 计算贝塞尔控制点（中点稍微下移，形成自然弧度）
        let mid = cc.v2((start.x + end.x) / 2, (start.y + end.y) / 2 - 50);

        // 设置线条样式
        this.graphics.lineWidth = 3; // 线宽
        this.graphics.strokeColor = this.lineColor; // 颜色

        // 绘制贝塞尔曲线
        this.graphics.moveTo(start.x, start.y);
        this.graphics.quadraticCurveTo(mid.x, mid.y, end.x, end.y);
        this.graphics.stroke();
    }


    update(dt) {
        if (!this.drlineflag) return
        this.drawFishingLine();
    }
}
