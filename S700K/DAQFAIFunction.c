// #########################################################################
// *************************************************************************
// 快速数据采集
// #########################################################################
// *************************************************************************

#include <cvirte.h>     
#include <userint.h>
#include "S700K.h"
#include "ADPCI1711.h"
#include "ADPCI1730.h"
#include "ADPCI1784.h"
#include "inifile.h"
#include "Global.h"

//------------------------------------------------------------------------------------------------
// 定义全局变量
//------------------------------------------------------------------------------------------------
extern int              plMain;
extern PT_DAQCard       g_PCI1711;
//extern PT_DAQCard       g_PCI1730;
extern PT_DAQCard       g_PCI1784;
extern PT_Record        g_S700KData;
extern PT_DAQData       g_DAQData;
extern PT_DAQSystemData g_DAQSystemData;
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// 数据变换，将输入的原始数据变为电压
// 输入:
//    reading     原始读入值
//    MaxVolt     最大电压值
//    MaxCount    最大原始值
//    offset      偏置值
// 输出:
//    无
// 返回:
//                变换后的电压
//    
//------------------------------------------------------------------------------------------------
float __stdcall DAQFAIScale(USHORT reading, FLOAT MaxVolt, USHORT MaxCount, USHORT offset)
{
   float    voltage;
   if (MaxCount == offset)
      return 0.0;

   // 线性变换
   voltage = (float)(MaxVolt * ((float)reading - (float)offset)) / (MaxCount - offset);

   return voltage;
}

//------------------------------------------------------------------------------------------------
// 数据变换，将输入的原始数据变为电压
// 输入:
//    reading     原始读入值
//    MaxVolt     最大电压值
//    MaxCount    最大原始值
//    offset      偏置值
// 输出:
//    无
// 返回:
//                变换后的电压
//    
//------------------------------------------------------------------------------------------------
float __stdcall DAQFAIScaleForce(USHORT reading)
{
   float    voltage;

   // 线性变换
   voltage = ((float)reading - 2048.0) / 2048.0;
   
   //printf("%d.        %f,\n", reading, voltage);
   
   voltage = (voltage - 0.5) * 20;

  
   return voltage;
}

//------------------------------------------------------------------------------------------------
// 从中断采集数据缓冲区中读入数据
// 输入:
//    ulRetrieved 已有AD数据个数
//    ushChannel  采集通道数
//    ulTimes     取平均值点数
// 输出:
//    无
// 返回:
//                原始值平均数
//    
//------------------------------------------------------------------------------------------------
USHORT __stdcall DAQFAIReading(ULONG ulRetrieved, USHORT ushChannel, ULONG ulTimes)
{
   ULONG          usReading = 0;
   static USHORT* psDataBuffer;
   ULONG          ulOffset;
   ULONG          i;
   
   // 参数校正
   if (ulTimes > g_PCI1711.ulPacerRate)
      return 2048;
   
   if (ushChannel > g_PCI1711.usNumber)
      return 2048;
   
   if (ulRetrieved > g_PCI1711.ulBufferSize)
      return 2048;
   
   if (g_PCI1711.AcquisitionBuffer == NULL)
      return 2048;
   
   // 平均值点数
   if (ulTimes <= 0)
      ulTimes = 1;

   // 取得中断数据采集缓冲区
   psDataBuffer = (USHORT*)g_PCI1711.AcquisitionBuffer;
   
   ulOffset = ulRetrieved;
   
   // 数据累加
   for (i = 0; i < ulTimes; i ++)
   {
      // Get Offset
      if (ulOffset > 0) 
         ulOffset = ulOffset - FAI_ChannelNumber;
      // Rewind to tail of data buffer
      else
         ulOffset = g_PCI1711.ulBufferSize - FAI_ChannelNumber;
      
      usReading += (psDataBuffer [ulOffset + ushChannel]) & 0xfff;
   }
   // 返回平均值
   return (USHORT)(usReading / ulTimes);
}



float __stdcall ReadForce()
{
	float val = 0.;
	PT_AIVoltageIn  m_ptAIVoltageIn;
	m_ptAIVoltageIn.chan = AICH_Force;
    m_ptAIVoltageIn.gain = 0;
    m_ptAIVoltageIn.TrigMode = 0;               // internal trigger
    m_ptAIVoltageIn.voltage = (FLOAT far *)&val;

    DRV_AIVoltageIn(g_PCI1711.hDevice, (LPT_AIVoltageIn)&m_ptAIVoltageIn);
	
	return (val-2.5)*4;
}


//------------------------------------------------------------------------------------------------
// 返回单次测量过程中的最大采样点，用于Load2最大力测量
// 输入:
//    无
// 输出:
//    无
// 返回:
//                最大力的电压值
//    
//------------------------------------------------------------------------------------------------
double __stdcall FAI_FindMaxForce(void)
{
   return g_DAQSystemData.fMaxForce;
}                                
//------------------------------------------------------------------------------------------------
// 返回单次测量过程中的最小采样点，用于Load2最大力测量
// 输入:
//    无
// 输出:
//    无
// 返回:
//                最大力的电压值
//    
//------------------------------------------------------------------------------------------------

double __stdcall FAI_FindMinForce(void)
{
   return g_DAQSystemData.fMinForce;
}                                

//------------------------------------------------------------------------------------------------
// 从中断采集数据缓冲区中读入力数据
// 输入:
//    ulTimes     取平均值点数
// 输出:
//    无
// 返回:
//                力的电压值
//    
//------------------------------------------------------------------------------------------------
double __stdcall DAQFAIReadForce(int nTime)
{
   LRESULT        ErrCde;           // Return error code
   double         dfValue;
   float          fMaxVolt = 10.0 * 40.0 / 10.0;
   USHORT         usOffset = 4096 * 3 / 4;
   USHORT         usReading;
   int            i;
   static USHORT  usActiveBuf;      // return by FAICheck
   static USHORT  usOverrun;        // return by FAICheck, FAITransfer
   static USHORT  usStopped;        // return by FAICheck
   static ULONG   ulRetrieved;      // return by FAICheck
   static USHORT  usHalfReady;      // return by FAICheck
   static PT_FAICheck      ptFAICheck;

   // 读取中断采集状态
   ptFAICheck.ActiveBuf = &usActiveBuf;
   ptFAICheck.stopped   = &usStopped;
   ptFAICheck.retrieved = &ulRetrieved;
   ptFAICheck.overrun   = &usOverrun;
   ptFAICheck.HalfReady = &usHalfReady;
   
   ErrCde = DRV_FAICheck (g_PCI1711.hDevice, &ptFAICheck);
   if (ErrCde != 0)
   {
	  ERR1(" ===PCI1713U故障===。DAQFAIReadForce函数内DRV_FAICheck调用失败，错误码：%d",ErrCde);	   
      DRV_FAIStop (g_PCI1711.hDevice);
      DRV_DeviceClose ((LONG *)&g_PCI1711.hDevice);
      g_PCI1711.hDevice = 0L;
      return FALSE;
   }

   // 偏移对齐
   ulRetrieved = ulRetrieved - ulRetrieved % FAI_ChannelNumber;

   // 读取Force
   usReading = DAQFAIReading (ulRetrieved, AICH_Force, 100);
   // 线性变换
   dfValue = DAQFAIScale (usReading, fMaxVolt, 4095, usOffset);

   // 返回绝对值
   return fabs (dfValue);
}


//------------------------------------------------------------------------------------------------
// 快速读取模拟量输入值
// 输入:
//    pData       保存采集数据的结构体
// 输出:
//    无
// 返回:
//    TRUE        成功
//    FALSE       失败
//------------------------------------------------------------------------------------------------
BOOL __stdcall FAI_ReadAIData( LPT_DAQData pData )
{
   LRESULT        ErrCde;           // Return error code
   char 		  ErrMessage[40];       // 返回的错误描述
   
   double         dfValue;
   USHORT         usOffset = 4096 * 2 / 4;
   USHORT         usReading;
   int            i;
   static USHORT  usActiveBuf;      // return by FAICheck
   static USHORT  usOverrun;        // return by FAICheck, FAITransfer
   static USHORT  usStopped;        // return by FAICheck
   static ULONG   ulRetrieved;      // return by FAICheck
   static USHORT  usHalfReady;      // return by FAICheck
   static PT_FAICheck      ptFAICheck;

   // 读取中断采集状态
   ptFAICheck.ActiveBuf = &usActiveBuf;
   ptFAICheck.stopped   = &usStopped;
   ptFAICheck.retrieved = &ulRetrieved;
   ptFAICheck.overrun   = &usOverrun;
   ptFAICheck.HalfReady = &usHalfReady;
   
   ErrCde = DRV_FAICheck (g_PCI1711.hDevice, &ptFAICheck);
   if (ErrCde != 0)
   {
	  DRV_GetErrorMessage(ErrCde,ErrMessage);
	  ERR1(" ===PCI1713U故障===。FAI_ReadAIData函数内DRV_FAICheck调用失败，错误码：%d.错误描述：%s",ErrCde,ErrMessage);
	  
      DRV_FAIStop (g_PCI1711.hDevice);
      DRV_DeviceClose ((LONG *)&g_PCI1711.hDevice);
      g_PCI1711.hDevice = 0L;
//      return FALSE;
   }

   // 偏移对齐
   ulRetrieved = ulRetrieved - ulRetrieved % FAI_ChannelNumber;

   // 读取Uab, Ubc, Uca电压
   usReading = DAQFAIReading (ulRetrieved, AICH_Uab, 200);
   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
   dfValue *= ( 1000.0 / 10.0 ) * 1 + 0.0;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfUab = dfValue;
   
   // 读取Uab, Ubc, Uca电压
   usReading = DAQFAIReading (ulRetrieved, AICH_Ubc, 200);
   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
   dfValue *= ( 1000.0 / 10.0 ) * 1 + 0.0;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfUbc = dfValue;
   
   // 读取Uab, Ubc, Uca电压
   usReading = DAQFAIReading (ulRetrieved, AICH_Uca, 200);
   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
   dfValue *= ( 1000.0 / 10.0 ) * 1 + 0.0;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfUca = dfValue;
   
//  读取Udc电压
//   usReading = DAQFAIReading (ulRetrieved, AICH_Udc, 1);
//   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
//   dfValue *= ( 200.0 / 5.0 ) + 0.0;
//   if ( dfValue < 0.0 )dfValue = 0.0;
//   pData->Data.dfUdc = dfValue;
  // 读取Idc
//   usReading = DAQFAIReading (ulRetrieved, AICH_Idc, 1);
//   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
//   dfValue *= 10.0 / 5.0;
//   if ( dfValue < 0.0 )dfValue = 0.0;
//   pData->Data.dfIdc = dfValue;

   
   // 读取Ia, Ib, Ic
   usReading = DAQFAIReading (ulRetrieved, AICH_Ia, 200);
   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
   dfValue *= 10.0 / 5.0;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfIa = dfValue;

   // 读取Ia, Ib, Ic
   usReading = DAQFAIReading (ulRetrieved, AICH_Ib, 200);
   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
   dfValue *= 10.0 / 5.0;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfIb = dfValue;

   // 读取Ia, Ib, Ic
   usReading = DAQFAIReading (ulRetrieved, AICH_Ic, 200);
   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
   dfValue *= 10.0 / 5.0;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfIc = dfValue;

   // 计算功率
   dfValue = ((pData->Data.dfUab + pData->Data.dfUbc + pData->Data.dfUca) / 3.0);
   dfValue *= ((pData->Data.dfIa + pData->Data.dfIb + pData->Data.dfIc) / 3.0);
   dfValue *= 1.732;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfPower = dfValue;
   
   // 读取力
   
   usReading = DAQFAIReading (ulRetrieved, AICH_Force, 200);
   dfValue = DAQFAIScaleForce (usReading);
   if (dfValue < -50.0)
      dfValue = -50.0;
   if (dfValue > 50.0)
      dfValue = 50.0;
   pData->Data.dfForce = fabs( dfValue );
   
   //dfValue = ReadForce();
   //pData->Data.dfForce = dfValue;
   if (g_DAQSystemData.fMaxForce < fabs(dfValue))
   		g_DAQSystemData.fMaxForce = fabs(dfValue);
   // 保存最小值。
   if (g_DAQSystemData.fMinForce > fabs(dfValue))
   		g_DAQSystemData.fMinForce = fabs(dfValue);
   
   // 读取位移
   ADPCI1784_PIValue( &g_PCI1784, &dfValue );
   if (dfValue < 0.0)dfValue = 0.0;
   if (dfValue > 3500.0)dfValue = 3500.0;
   pData->Data.dfStroke = dfValue;
   
   // 正常返回
   return TRUE;
}

//------------------------------------------------------------------------------------------------
// 中断数据采集时，数据缓冲区改变通知回调函数
// 输入:
//    无
// 输出:
//    无
// 返回:
//    无
//------------------------------------------------------------------------------------------------
void __stdcall DAQFAIOnBufferChange(void)
{
   char           szData[32];
   LRESULT        ErrCde;           // Return error code
   char           szErrMsg[80];     // Use for MESSAGEBOX function
   USHORT         usReading;
   float          dfValue;
   
   static float   fMaxVolt = 10.0 * 50.0 / 10.0;
   static USHORT  usOffset = 2048;

   static USHORT  usActiveBuf;      // return by FAICheck
   static USHORT  usOverrun;        // return by FAICheck, FAITransfer
   static USHORT  usStopped;        // return by FAICheck
   static ULONG   ulRetrieved;      // return by FAICheck
   static USHORT  usHalfReady;      // return by FAICheck
   static float*  pfDataBuffer;
   static float*  pfCopyBuffer;
   static ULONG   ulOffset;         //
   static ULONG   ulDataLen;
   static PT_FAICheck      ptFAICheck;

   // 读取中断采集状态
   ptFAICheck.ActiveBuf = &usActiveBuf; // 已激活的buffer 
   ptFAICheck.stopped   = &usStopped;   // 指示操作是否结束（1－>结束，0 —>未结束）
   ptFAICheck.retrieved = &ulRetrieved; // 内存中保存的A/D转换次数
   ptFAICheck.overrun   = &usOverrun;   // 内存中的数据是否超过循环模式要求的量，即是否已经是第二次以后的循环
   ptFAICheck.HalfReady = &usHalfReady; // 半满标志

   ErrCde = DRV_FAICheck (g_PCI1711.hDevice, &ptFAICheck);
   if (ErrCde != 0)
   {  
	  ERR1(" ===PCI1713U故障===。DAQFAIReadForce函数内DRV_FAICheck调用失败，错误码：%d",ErrCde);	   	   
      DRV_FAIStop (g_PCI1711.hDevice);
      DRV_DeviceClose ((LONG *)&g_PCI1711.hDevice);
      g_PCI1711.hDevice = 0L;
      return;
   }

   // 计算正确的缓冲区位置
   pfCopyBuffer = (float*)g_PCI1711.CopyBuffer;
   
   ulDataLen = g_PCI1711.ulBufferSize / 2;

   if (1 == usHalfReady)
      ulRetrieved = 0;
   else
      ulRetrieved = ulDataLen;

   ulDataLen = g_PCI1711.ulPacerRate / 2;
   pfDataBuffer = (float*)g_DAQSystemData.DataBuffer;
   
   // 保存原有数据
   memcpy (pfDataBuffer, pfDataBuffer + ulDataLen, sizeof (float) * ulDataLen);
   pfDataBuffer += ulDataLen;

   // 计算数据,50ms滑动平均
   for (ulOffset = 0; ulOffset < ulDataLen; ulOffset ++)
   {
      usReading = DAQFAIReading (ulRetrieved + ulOffset * FAI_ChannelNumber, AICH_Force, 50);
      dfValue = DAQFAIScale (usReading, fMaxVolt, 4095, usOffset);
      pfDataBuffer [ulOffset] = dfValue;
   }
   
   // 计算单次最大力电压
   pfDataBuffer = g_DAQSystemData.DataBuffer;
   for (ulOffset = 0; ulOffset < g_PCI1711.ulPacerRate; ulOffset ++)
   {
      dfValue = fabs (pfDataBuffer [ulOffset]);
      //if (g_DAQSystemData.fMaxForce < dfValue)
         //g_DAQSystemData.fMaxForce = dfValue;
   }

   // 调整计数值
   g_PCI1711.ulBufferChanged ++;

   return;
}

//---------------------------------------------------------------------------
// 中断数据采集时，数据溢出时通知回调函数
// 输入:
//    无
// 输出:
//    无
// 返回:
//    无
//------------------------------------------------------------------------------------------------
void __stdcall DAQFAIOnOverrun( void )
{
   g_PCI1711.ulOverruned ++;
   //ERR1("-AD采集卡内部缓冲区溢出，这是第%d次溢出。",g_PCI1711.ulOverruned);
   DRV_ClearOverrun (g_PCI1711.hDevice);
}

//---------------------------------------------------------------------------
// 中断数据采集时，数据采集停止时通知回调函数
// 输入:
//    无
// 输出:
//    无
// 返回:
//    无
//------------------------------------------------------------------------------------------------
void __stdcall DAQFAIOnTerminate( void )
{
   LRESULT           ErrCde;        // Return error code

   // 关闭数据采集卡
   ErrCde = DRV_DeviceClose ((LONG far *)&g_PCI1711.hDevice);
   if (ErrCde != SUCCESS)
      ;
   ERR1("-AD采集卡终止事件ADS_EVT_TERMINATED，关闭采集卡。停止采集");
}

//------------------------------------------------------------------------------------------------
// 快速数据采集监测献策
// 输入:
//    arg         无用
// 输出:
//    无
// 返回:
//    0           成功
//------------------------------------------------------------------------------------------------
DWORD WINAPI DAQFAIRoutine(DWORD arg)
{
   LRESULT           ErrCde;        // Return error code
   char				 lpszErrMsg[256];		// 返回的错误描述   
   USHORT            usEventType;
   char              szData[32];
   PT_CheckEvent     ptCheckEvent;
   
   
   // 如果条件满足时，循环运行
   while (g_DAQSystemData.usDAQThreadLoop)
   {
      // 检测通知事件
      ptCheckEvent.EventType = &usEventType;
      ptCheckEvent.Milliseconds = 1000;

      ErrCde = DRV_CheckEvent (g_PCI1711.hDevice, &ptCheckEvent);
	  if (ErrCde != SUCCESS){
		  DRV_GetErrorMessage(ErrCde,lpszErrMsg);
		  ERR1("-AD采集卡查询事件函数DRV_CheckEvent出错，错误代码：%d。驱动程序返回错误描述：%s",ErrCde,lpszErrMsg);
         continue;
	  }

      // 处理缓冲区改变事件
      if (usEventType & ADS_EVT_BUFCHANGE)
         DAQFAIOnBufferChange ();

      // 处理溢出事件
      if (usEventType & ADS_EVT_OVERRUN)
         DAQFAIOnOverrun ();

      // 处理终止事件
      if (usEventType & ADS_EVT_TERMINATED)
      {
         DAQFAIOnTerminate ();
         break;	   // 退出采集线程
      }
   }
   return 0;
}

//------------------------------------------------------------------------------------------------
// 启动快速数据采集
// 输入:
//    无
// 输出:
//    无
// 返回:
//    0           成功
//    <0          失败
//------------------------------------------------------------------------------------------------
int __stdcall FAI_ScanStart(void)
{
   LRESULT              ErrCde;        // Return error code
   char                 szErrMsg[256];
   unsigned short       ausGainCode[16];
   int                  i;
   PT_FAIIntScanStart   ptFAIIntScanStart;
   PT_EnableEvent       ptEnableEvent;
   long                 ulBufferSize;
   
   // 分配中断采集方式数据保存缓冲区，系统驱动使用
   ulBufferSize = g_PCI1711.ulBufferSize * sizeof (USHORT);
   if ((g_PCI1711.AcquisitionBuffer = (USHORT far *)GlobalAlloc (GPTR, ulBufferSize)) == 0)
   {
      lstrcpy ((char *)szErrMsg, "全局内存不足,不能够分配内存!");
      MessageBox (NULL, (char *)szErrMsg, "错误", MB_OK);
      return -1;
   }

   // 分配拷贝缓冲区
   ulBufferSize = g_PCI1711.ulBufferSize * sizeof (USHORT);
   if ((g_PCI1711.CopyBuffer = (USHORT far *)GlobalAlloc (GPTR, ulBufferSize)) == 0)
   {
      lstrcpy ((char *)szErrMsg, "全局内存不足,不能够分配内存!");
      MessageBox (NULL, (char *)szErrMsg, "错误", MB_OK);
      return -1;
   }

   // 设置采集增益
   for (i = 0; i < 16; i ++)
      ausGainCode [i] = g_PCI1711.usGainCode;

   // 如果采集卡没有打开，返回错误
   if (g_PCI1711.hDevice == 0L)
      return -1;

   // 运行事件检测
   ptEnableEvent.EventType = ADS_EVT_BUFCHANGE | ADS_EVT_TERMINATED | ADS_EVT_OVERRUN;
   ptEnableEvent.Enabled = TRUE;
   ptEnableEvent.Count = 2048;

   ErrCde = DRV_EnableEvent (g_PCI1711.hDevice, &ptEnableEvent);
   if (ErrCde != SUCCESS)
   {
      DRV_DeviceClose ((LONG far *)&g_PCI1711.hDevice);
      g_PCI1711.hDevice = 0L;
      return -1;
   }
   
   // 启动数据中断扫描
   ptFAIIntScanStart.TrigSrc        = 0;  // —设置触发源（0，内部；1，外部）；
   ptFAIIntScanStart.cyclic         = 1;  // —是否循环（0－非循环，1－循环）
   ptFAIIntScanStart.StartChan      = g_PCI1711.usStart;
   ptFAIIntScanStart.NumChans       = g_PCI1711.usNumber;
   ptFAIIntScanStart.SampleRate     = g_PCI1711.ulBufferSize;  // 16K/s
   ptFAIIntScanStart.GainList       = &ausGainCode[ 0 ];
   ptFAIIntScanStart.count          = g_PCI1711.ulBufferSize;  // 16K
   ptFAIIntScanStart.IntrCount      = 2048;  // 当使用FIFO时，该值必须是FIFO大小的一半（FIFO_SIZE）。
   ptFAIIntScanStart.buffer         = (USHORT *)g_PCI1711.AcquisitionBuffer;

   ErrCde = DRV_FAIIntScanStart (g_PCI1711.hDevice, &ptFAIIntScanStart);
   if (ErrCde != SUCCESS)
   {
      DRV_DeviceClose ((LONG far *)&g_PCI1711.hDevice);
      g_PCI1711.hDevice = 0L;
      return -1;
   }
   g_PCI1711.ulBufferChanged = 0;
   g_PCI1711.ulOverruned = 0;

   // 设置守护线程运行位
   g_DAQSystemData.usDAQThreadLoop = TRUE;
   
   // 建立守护线程
   g_DAQSystemData.uhFAIThread = CreateThread (NULL, 0,
      (LPTHREAD_START_ROUTINE)&DAQFAIRoutine, NULL, 0, NULL);
   
   // 如果守护线程没有启动，返回错误
   if (g_DAQSystemData.uhFAIThread == NULL)
      return -1;
   
   // 正确返回
   return 0;
}

//------------------------------------------------------------------------------------------------
// 停止快速数据采集
// 输入:
//    无
// 输出:
//    无
// 返回:
//    0           成功
//    <0          失败
//------------------------------------------------------------------------------------------------
int __stdcall FAI_ScanStop(void)
{
   int   ErrCde;
   
   // 要求守护线程结束
   g_DAQSystemData.usDAQThreadLoop = FALSE;

   // 终止数据中断扫描
   if (g_PCI1711.hDevice != 0L)
   {
      ErrCde = DRV_FAITerminate (g_PCI1711.hDevice);
      if (ErrCde != SUCCESS)
      {
      }
   }

   // 等待守护线程结束
   WaitForSingleObject (g_DAQSystemData.uhFAIThread, 2000);
   CloseHandle (g_DAQSystemData.uhFAIThread);
   g_DAQSystemData.uhFAIThread = NULL;
      // 释放缓冲区
   if (g_PCI1711.AcquisitionBuffer != NULL)
   {
      GlobalFree (g_PCI1711.AcquisitionBuffer);
      g_PCI1711.AcquisitionBuffer = NULL;
   }
   
   // 释放缓冲区
   if (g_PCI1711.CopyBuffer != NULL)
   {
      GlobalFree (g_PCI1711.CopyBuffer);
      g_PCI1711.CopyBuffer = NULL;
   }
   return 0;
}

//------------------------------------------------------------------------------------------------
