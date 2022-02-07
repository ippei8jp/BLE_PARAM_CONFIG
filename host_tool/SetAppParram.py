import sys
import time
import atexit               # 終了時処理用

# bluetooth操作用
import bluepy

"""
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
パラメータなしの場合は現在の設定値をリードして表示
パラメータ3個の場合は指定された値をライトし、その結果をリードして表示
それ以外はエラー終了

root権限での実行(sudo) 必須。

動作確認用なので、結構いいかげんな作りです。ごめんなさい。
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
"""
# #### BLE ParamConfig クラス ###################################################
class PARAM_CONFIG() :
    # ターゲットデバイスのCHARACTERISTIC_UUID
    SERVICE_UUID                    = bluepy.btle.UUID('ea7542b0-bfae-7587-dc60-45dbf29ca088')
    CHARACTERISTIC_UUID_SSID_NAME   = bluepy.btle.UUID('ea7542b1-bfae-7587-dc60-45dbf29ca088')
    CHARACTERISTIC_UUID_SSID_PASS   = bluepy.btle.UUID('ea7542b2-bfae-7587-dc60-45dbf29ca088')
    CHARACTERISTIC_UUID_LOOP_ITVL   = bluepy.btle.UUID('ea7542b3-bfae-7587-dc60-45dbf29ca088')

    # 有効なデータ長(こちらから調べる方法はある?)
    CHARACTERISTIC_LEN_SSID_NAME   = 15
    CHARACTERISTIC_LEN_SSID_PASS   = 15
    CHARACTERISTIC_LEN_LOOP_ITVL   = 4
    
    # ==== 初期化 ============================================================================================
    def __init__(self, dev_name, device) :
        print(f'**** PARAM_CONFIG constructor  {dev_name}   addr : {device.addr},   addrType : {device.addrType}')
        self.dev_name = dev_name
        self.device   = device
        
        # 接続済みフラグ
        self.isConnected = False
        
        # とりあえず初期化
        self.peri = None
        self.service = None
        self.descs = []
    
    def searchDescriptor(self, uuid) :
        return next((desc for desc in self.descs if desc.uuid == uuid ), None)
        """
        参考：
        リストの中のインスタンスのもつ値で検索
        https://qiita.com/endorno/items/ba59c82a39df0336e25f
        """
    
    # ==== 接続 ==============================================================================================
    def connect(self):
        if not self.device :
            # デバイスがない
            print("**ERROR** device not found!!")
            return
        
        if self.isConnected :
            # 接続済み
            print("**WARNING** {self.dev_name} already connected!!")
            return
        
        try:
            self.peri = bluepy.btle.Peripheral()
            
            # 接続
            self.peri.connect(self.device)
        except:
            print("device connect error")
            return
        
        # 接続済みフラグ
        self.isConnected = True
        
        # サービス取得
        self.peri.discoverServices()                                        # これがないとダメっぽい
        self.service = self.peri.getServiceByUUID(self.SERVICE_UUID)        # 対象サービスを取得
        
        # サービス内のディスクリプタを取得
        self.descs = self.service.getDescriptors()
    
    # ==== 読み出し(共通) ==============================================================================================
    def read(self, uuid) :
        data = None
        if self.isConnected :
            desc = self.searchDescriptor(uuid)      # 取得したディスクリプタから指定したUUIDを探す
            if desc :                               # 見つかった？
                data = desc.read()
                print(f"READ    {uuid}   {data}")
        return data
    
    # ==== 書き込み(共通) ==============================================================================================
    def write(self, uuid, data, size=1, withResponse=False) :
        if self.isConnected :
            desc = self.searchDescriptor(uuid)      # 取得したディスクリプタから指定したUUIDを探す
            if desc :                               # 見つかった？
                if isinstance(data, str) :
                    data = data.encode()                            # dataが文字列だったらバイト列に変換
                elif isinstance(data, int) :
                    data = data.to_bytes(size, byteorder="little")  # dataがintだったらバイト列に変換
                print(f"WRITE    {uuid}   {data}")
                desc.write(data, withResponse)
    
    # ==== 読み出し(SSID name) ==============================================================================================
    def readSsidName(self) :
        uuid = self.CHARACTERISTIC_UUID_SSID_NAME
        data = self.read(uuid)
        val = data.decode('ascii')
        return val

    # ==== 読み出し(SSID pass) ==============================================================================================
    def readSsidPass(self) :
        uuid = self.CHARACTERISTIC_UUID_SSID_PASS
        data = self.read(uuid)
        val = data.decode('ascii')
        return val

    # ==== 読み出し(Loop interval) ==============================================================================================
    def readLoopItvl(self) :
        uuid = self.CHARACTERISTIC_UUID_LOOP_ITVL
        data = self.read(uuid)
        val  = int.from_bytes(data, byteorder='little', signed=False)    # int型に変換
        return val

    # ==== 書き込み(SSID name) ==============================================================================================
    def writeSsidName(self, data) :
        uuid = self.CHARACTERISTIC_UUID_SSID_NAME
        len  = self.CHARACTERISTIC_LEN_SSID_NAME
        self.write(uuid, data, len)

    # ==== 書き込み(SSID pass) ==============================================================================================
    def writeSsidPass(self, data) :
        uuid = self.CHARACTERISTIC_UUID_SSID_PASS
        len  = self.CHARACTERISTIC_LEN_SSID_PASS
        self.write(uuid, data, len)

    # ==== 書き込み(Loop interval) ==============================================================================================
    def writeLoopItvl(self, data) :
        uuid = self.CHARACTERISTIC_UUID_LOOP_ITVL
        len  = self.CHARACTERISTIC_LEN_LOOP_ITVL
        self.write(uuid, data, len)

    # ==== 切断 ==============================================================================================
    def disconnect(self) :
        if self.isConnected :
            self.peri.disconnect()
            print(f'{self.dev_name}  **** disconnected ****')
            # 接続済みフラグ
            self.isConnected = False
# ======================================================================================================================================

def main() :
    # コマンドラインパラメータの処理   ... なんて やっつけな実装なんだ....
    write_flag = False          # 書き込みフラグは落としておく
    num_arg = len(sys.argv)
    if num_arg == 1 :
        # パラメータなし
        pass
    elif num_arg == 4 :
        # パラメータ3個
        name = str(sys.argv[1])
        pswd = str(sys.argv[2])
        itvl = int(sys.argv[3])
        write_flag = True       # 書き込みフラグを立てる
    else :
        print("**** ERROR **** Must have 3 parameters")
        sys.exit(1)
    
    # デバイスのスキャン
    print(f"Searching BLE peripherals...")
    scanner = bluepy.btle.Scanner(0)
    devices = scanner.scan(3)      # 3秒間スキャンする
    
    # advertising dataを確認
    if False :       # debug用
        print("=========================================")
        for device in devices:
            print(f'BD_ADDR: {device.addr}')
            # スキャンデータを表示してみる
            for (sdid, desc, value) in device.getScanData():
                print(f'{sdid}    {desc}    {value}')
            print("=========================================")
        # sys.exit(1)
    
    # サーチ
    param_configs = []
    for device in devices:
        # とりあえず128bit UUIDを読んでみる
        sv_uuid_str = device.getValueText(bluepy.btle.ScanEntry.COMPLETE_128B_SERVICES)          # Complete 128b Services
        if  sv_uuid_str is None:
            # 128bit UUIDがなければ16bit UUIDを読んでみる
            sv_uuid_str = device.getValueText(bluepy.btle.ScanEntry.COMPLETE_16B_SERVICES)          # Complete 16b Services
        dev_name   = device.getValueText(bluepy.btle.ScanEntry.COMPLETE_LOCAL_NAME)             # Complete Local Name
        if  dev_name is None:
            # Complete Local NameがなければShort Local Nameを読んでみる
            dev_name = device.getValueText(bluepy.btle.ScanEntry.SHORT_LOCAL_NAME)              # Short Local Name
        # debug用
        print(f'BD_ADDR: {device.addr}({device.addrType})    UUID: {sv_uuid_str}    name: {dev_name}')
        
        if sv_uuid_str is None :
            # UUIDが定義されていなければスキップ
            continue
        
        # UUID型に変換
        sv_uuid = bluepy.btle.UUID(sv_uuid_str)
        
        # UUIDが一致?
        if sv_uuid == PARAM_CONFIG.SERVICE_UUID :
            # 見つかった
            # PARAM_CONFIG のインスタンス生成
            param_configs.append(PARAM_CONFIG(dev_name, device))
    
    if len(param_configs) == 0 :
        # PARAM_CONFIG が見つからなかった
        print("#### PARAM_CONFIG module not found ####")
        sys.exit(1)
    
    param_config = param_configs[0]        # とりあえず最初の1個だけ使う
    
    # 接続
    print('==== connect ====')
    param_config.connect()
    
    try :
        if write_flag :
            # ==== 書き込み(SSID name) ==============================================================================================
            param_config.writeSsidName(name)
            time.sleep(1)                           # アクセスの間隔は少しあけたほうがよさそう
            # ==== 書き込み(SSID pass) ==============================================================================================
            param_config.writeSsidPass(pswd)
            time.sleep(1)                           # アクセスの間隔は少しあけたほうがよさそう
            # ==== 書き込み(Loop interval) ==============================================================================================
            param_config.writeLoopItvl(itvl)
            time.sleep(1)                           # アクセスの間隔は少しあけたほうがよさそう
        
        # read
        # ==== 読み出し(SSID name) ==============================================================================================
        name = param_config.readSsidName()
        time.sleep(1)                               # アクセスの間隔は少しあけたほうがよさそう
        # ==== 読み出し(SSID pass) ==============================================================================================
        pswd = param_config.readSsidPass()
        time.sleep(1)                               # アクセスの間隔は少しあけたほうがよさそう
        # ==== 読み出し(Loop interval) ==============================================================================================
        itvl = param_config.readLoopItvl()
        time.sleep(1)                               # アクセスの間隔は少しあけたほうがよさそう
        
        print('====================================================')
        print(f'SSID name     : "{name}"')
        print(f'SSID pass     : "{pswd}"')
        print(f'Loop Interval : {itvl}')
        print('====================================================')
    
    except Exception as e:
        print("******** Read/Write Error ********")
        print(e)
    
        # 切断
    print("==== disconnect ====")
    param_config.disconnect()

main()
