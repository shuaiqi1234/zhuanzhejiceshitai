// #########################################################################
// *************************************************************************
// �������ݲɼ�
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
// ����ȫ�ֱ���
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
// ���ݱ任���������ԭʼ���ݱ�Ϊ��ѹ
// ����:
//    reading     ԭʼ����ֵ
//    MaxVolt     ����ѹֵ
//    MaxCount    ���ԭʼֵ
//    offset      ƫ��ֵ
// ���:
//    ��
// ����:
//                �任��ĵ�ѹ
//    
//------------------------------------------------------------------------------------------------
float __stdcall DAQFAIScale(USHORT reading, FLOAT MaxVolt, USHORT MaxCount, USHORT offset)
{
   float    voltage;
   if (MaxCount == offset)
      return 0.0;

   // ���Ա任
   voltage = (float)(MaxVolt * ((float)reading - (float)offset)) / (MaxCount - offset);

   return voltage;
}

//------------------------------------------------------------------------------------------------
// ���ݱ任���������ԭʼ���ݱ�Ϊ��ѹ
// ����:
//    reading     ԭʼ����ֵ
//    MaxVolt     ����ѹֵ
//    MaxCount    ���ԭʼֵ
//    offset      ƫ��ֵ
// ���:
//    ��
// ����:
//                �任��ĵ�ѹ
//    
//------------------------------------------------------------------------------------------------
float __stdcall DAQFAIScaleForce(USHORT reading)
{
   float    voltage;

   // ���Ա任
   voltage = ((float)reading - 2048.0) / 2048.0;
   
   //printf("%d.        %f,\n", reading, voltage);
   
   voltage = (voltage - 0.5) * 20;

  
   return voltage;
}

//------------------------------------------------------------------------------------------------
// ���жϲɼ����ݻ������ж�������
// ����:
//    ulRetrieved ����AD���ݸ���
//    ushChannel  �ɼ�ͨ����
//    ulTimes     ȡƽ��ֵ����
// ���:
//    ��
// ����:
//                ԭʼֵƽ����
//    
//------------------------------------------------------------------------------------------------
USHORT __stdcall DAQFAIReading(ULONG ulRetrieved, USHORT ushChannel, ULONG ulTimes)
{
   ULONG          usReading = 0;
   static USHORT* psDataBuffer;
   ULONG          ulOffset;
   ULONG          i;
   
   // ����У��
   if (ulTimes > g_PCI1711.ulPacerRate)
      return 2048;
   
   if (ushChannel > g_PCI1711.usNumber)
      return 2048;
   
   if (ulRetrieved > g_PCI1711.ulBufferSize)
      return 2048;
   
   if (g_PCI1711.AcquisitionBuffer == NULL)
      return 2048;
   
   // ƽ��ֵ����
   if (ulTimes <= 0)
      ulTimes = 1;

   // ȡ���ж����ݲɼ�������
   psDataBuffer = (USHORT*)g_PCI1711.AcquisitionBuffer;
   
   ulOffset = ulRetrieved;
   
   // �����ۼ�
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
   // ����ƽ��ֵ
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
// ���ص��β��������е��������㣬����Load2���������
// ����:
//    ��
// ���:
//    ��
// ����:
//                ������ĵ�ѹֵ
//    
//------------------------------------------------------------------------------------------------
double __stdcall FAI_FindMaxForce(void)
{
   return g_DAQSystemData.fMaxForce;
}                                
//------------------------------------------------------------------------------------------------
// ���ص��β��������е���С�����㣬����Load2���������
// ����:
//    ��
// ���:
//    ��
// ����:
//                ������ĵ�ѹֵ
//    
//------------------------------------------------------------------------------------------------

double __stdcall FAI_FindMinForce(void)
{
   return g_DAQSystemData.fMinForce;
}                                

//------------------------------------------------------------------------------------------------
// ���жϲɼ����ݻ������ж���������
// ����:
//    ulTimes     ȡƽ��ֵ����
// ���:
//    ��
// ����:
//                ���ĵ�ѹֵ
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

   // ��ȡ�жϲɼ�״̬
   ptFAICheck.ActiveBuf = &usActiveBuf;
   ptFAICheck.stopped   = &usStopped;
   ptFAICheck.retrieved = &ulRetrieved;
   ptFAICheck.overrun   = &usOverrun;
   ptFAICheck.HalfReady = &usHalfReady;
   
   ErrCde = DRV_FAICheck (g_PCI1711.hDevice, &ptFAICheck);
   if (ErrCde != 0)
   {
	  ERR1(" ===PCI1713U����===��DAQFAIReadForce������DRV_FAICheck����ʧ�ܣ������룺%d",ErrCde);	   
      DRV_FAIStop (g_PCI1711.hDevice);
      DRV_DeviceClose ((LONG *)&g_PCI1711.hDevice);
      g_PCI1711.hDevice = 0L;
      return FALSE;
   }

   // ƫ�ƶ���
   ulRetrieved = ulRetrieved - ulRetrieved % FAI_ChannelNumber;

   // ��ȡForce
   usReading = DAQFAIReading (ulRetrieved, AICH_Force, 100);
   // ���Ա任
   dfValue = DAQFAIScale (usReading, fMaxVolt, 4095, usOffset);

   // ���ؾ���ֵ
   return fabs (dfValue);
}


//------------------------------------------------------------------------------------------------
// ���ٶ�ȡģ��������ֵ
// ����:
//    pData       ����ɼ����ݵĽṹ��
// ���:
//    ��
// ����:
//    TRUE        �ɹ�
//    FALSE       ʧ��
//------------------------------------------------------------------------------------------------
BOOL __stdcall FAI_ReadAIData( LPT_DAQData pData )
{
   LRESULT        ErrCde;           // Return error code
   char 		  ErrMessage[40];       // ���صĴ�������
   
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

   // ��ȡ�жϲɼ�״̬
   ptFAICheck.ActiveBuf = &usActiveBuf;
   ptFAICheck.stopped   = &usStopped;
   ptFAICheck.retrieved = &ulRetrieved;
   ptFAICheck.overrun   = &usOverrun;
   ptFAICheck.HalfReady = &usHalfReady;
   
   ErrCde = DRV_FAICheck (g_PCI1711.hDevice, &ptFAICheck);
   if (ErrCde != 0)
   {
	  DRV_GetErrorMessage(ErrCde,ErrMessage);
	  ERR1(" ===PCI1713U����===��FAI_ReadAIData������DRV_FAICheck����ʧ�ܣ������룺%d.����������%s",ErrCde,ErrMessage);
	  
      DRV_FAIStop (g_PCI1711.hDevice);
      DRV_DeviceClose ((LONG *)&g_PCI1711.hDevice);
      g_PCI1711.hDevice = 0L;
//      return FALSE;
   }

   // ƫ�ƶ���
   ulRetrieved = ulRetrieved - ulRetrieved % FAI_ChannelNumber;

   // ��ȡUab, Ubc, Uca��ѹ
   usReading = DAQFAIReading (ulRetrieved, AICH_Uab, 200);
   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
   dfValue *= ( 1000.0 / 10.0 ) * 1 + 0.0;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfUab = dfValue;
   
   // ��ȡUab, Ubc, Uca��ѹ
   usReading = DAQFAIReading (ulRetrieved, AICH_Ubc, 200);
   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
   dfValue *= ( 1000.0 / 10.0 ) * 1 + 0.0;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfUbc = dfValue;
   
   // ��ȡUab, Ubc, Uca��ѹ
   usReading = DAQFAIReading (ulRetrieved, AICH_Uca, 200);
   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
   dfValue *= ( 1000.0 / 10.0 ) * 1 + 0.0;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfUca = dfValue;
   
//  ��ȡUdc��ѹ
//   usReading = DAQFAIReading (ulRetrieved, AICH_Udc, 1);
//   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
//   dfValue *= ( 200.0 / 5.0 ) + 0.0;
//   if ( dfValue < 0.0 )dfValue = 0.0;
//   pData->Data.dfUdc = dfValue;
  // ��ȡIdc
//   usReading = DAQFAIReading (ulRetrieved, AICH_Idc, 1);
//   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
//   dfValue *= 10.0 / 5.0;
//   if ( dfValue < 0.0 )dfValue = 0.0;
//   pData->Data.dfIdc = dfValue;

   
   // ��ȡIa, Ib, Ic
   usReading = DAQFAIReading (ulRetrieved, AICH_Ia, 200);
   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
   dfValue *= 10.0 / 5.0;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfIa = dfValue;

   // ��ȡIa, Ib, Ic
   usReading = DAQFAIReading (ulRetrieved, AICH_Ib, 200);
   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
   dfValue *= 10.0 / 5.0;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfIb = dfValue;

   // ��ȡIa, Ib, Ic
   usReading = DAQFAIReading (ulRetrieved, AICH_Ic, 200);
   dfValue = DAQFAIScale (usReading, 5, 4095, usOffset);
   dfValue *= 10.0 / 5.0;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfIc = dfValue;

   // ���㹦��
   dfValue = ((pData->Data.dfUab + pData->Data.dfUbc + pData->Data.dfUca) / 3.0);
   dfValue *= ((pData->Data.dfIa + pData->Data.dfIb + pData->Data.dfIc) / 3.0);
   dfValue *= 1.732;
   if ( dfValue < 0.0 )dfValue = 0.0;
   pData->Data.dfPower = dfValue;
   
   // ��ȡ��
   
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
   // ������Сֵ��
   if (g_DAQSystemData.fMinForce > fabs(dfValue))
   		g_DAQSystemData.fMinForce = fabs(dfValue);
   
   // ��ȡλ��
   ADPCI1784_PIValue( &g_PCI1784, &dfValue );
   if (dfValue < 0.0)dfValue = 0.0;
   if (dfValue > 3500.0)dfValue = 3500.0;
   pData->Data.dfStroke = dfValue;
   
   // ��������
   return TRUE;
}

//------------------------------------------------------------------------------------------------
// �ж����ݲɼ�ʱ�����ݻ������ı�֪ͨ�ص�����
// ����:
//    ��
// ���:
//    ��
// ����:
//    ��
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

   // ��ȡ�жϲɼ�״̬
   ptFAICheck.ActiveBuf = &usActiveBuf; // �Ѽ����buffer 
   ptFAICheck.stopped   = &usStopped;   // ָʾ�����Ƿ������1��>������0 ��>δ������
   ptFAICheck.retrieved = &ulRetrieved; // �ڴ��б����A/Dת������
   ptFAICheck.overrun   = &usOverrun;   // �ڴ��е������Ƿ񳬹�ѭ��ģʽҪ����������Ƿ��Ѿ��ǵڶ����Ժ��ѭ��
   ptFAICheck.HalfReady = &usHalfReady; // ������־

   ErrCde = DRV_FAICheck (g_PCI1711.hDevice, &ptFAICheck);
   if (ErrCde != 0)
   {  
	  ERR1(" ===PCI1713U����===��DAQFAIReadForce������DRV_FAICheck����ʧ�ܣ������룺%d",ErrCde);	   	   
      DRV_FAIStop (g_PCI1711.hDevice);
      DRV_DeviceClose ((LONG *)&g_PCI1711.hDevice);
      g_PCI1711.hDevice = 0L;
      return;
   }

   // ������ȷ�Ļ�����λ��
   pfCopyBuffer = (float*)g_PCI1711.CopyBuffer;
   
   ulDataLen = g_PCI1711.ulBufferSize / 2;

   if (1 == usHalfReady)
      ulRetrieved = 0;
   else
      ulRetrieved = ulDataLen;

   ulDataLen = g_PCI1711.ulPacerRate / 2;
   pfDataBuffer = (float*)g_DAQSystemData.DataBuffer;
   
   // ����ԭ������
   memcpy (pfDataBuffer, pfDataBuffer + ulDataLen, sizeof (float) * ulDataLen);
   pfDataBuffer += ulDataLen;

   // ��������,50ms����ƽ��
   for (ulOffset = 0; ulOffset < ulDataLen; ulOffset ++)
   {
      usReading = DAQFAIReading (ulRetrieved + ulOffset * FAI_ChannelNumber, AICH_Force, 50);
      dfValue = DAQFAIScale (usReading, fMaxVolt, 4095, usOffset);
      pfDataBuffer [ulOffset] = dfValue;
   }
   
   // ���㵥���������ѹ
   pfDataBuffer = g_DAQSystemData.DataBuffer;
   for (ulOffset = 0; ulOffset < g_PCI1711.ulPacerRate; ulOffset ++)
   {
      dfValue = fabs (pfDataBuffer [ulOffset]);
      //if (g_DAQSystemData.fMaxForce < dfValue)
         //g_DAQSystemData.fMaxForce = dfValue;
   }

   // ��������ֵ
   g_PCI1711.ulBufferChanged ++;

   return;
}

//---------------------------------------------------------------------------
// �ж����ݲɼ�ʱ���������ʱ֪ͨ�ص�����
// ����:
//    ��
// ���:
//    ��
// ����:
//    ��
//------------------------------------------------------------------------------------------------
void __stdcall DAQFAIOnOverrun( void )
{
   g_PCI1711.ulOverruned ++;
   //ERR1("-AD�ɼ����ڲ���������������ǵ�%d�������",g_PCI1711.ulOverruned);
   DRV_ClearOverrun (g_PCI1711.hDevice);
}

//---------------------------------------------------------------------------
// �ж����ݲɼ�ʱ�����ݲɼ�ֹͣʱ֪ͨ�ص�����
// ����:
//    ��
// ���:
//    ��
// ����:
//    ��
//------------------------------------------------------------------------------------------------
void __stdcall DAQFAIOnTerminate( void )
{
   LRESULT           ErrCde;        // Return error code

   // �ر����ݲɼ���
   ErrCde = DRV_DeviceClose ((LONG far *)&g_PCI1711.hDevice);
   if (ErrCde != SUCCESS)
      ;
   ERR1("-AD�ɼ�����ֹ�¼�ADS_EVT_TERMINATED���رղɼ�����ֹͣ�ɼ�");
}

//------------------------------------------------------------------------------------------------
// �������ݲɼ�����ײ�
// ����:
//    arg         ����
// ���:
//    ��
// ����:
//    0           �ɹ�
//------------------------------------------------------------------------------------------------
DWORD WINAPI DAQFAIRoutine(DWORD arg)
{
   LRESULT           ErrCde;        // Return error code
   char				 lpszErrMsg[256];		// ���صĴ�������   
   USHORT            usEventType;
   char              szData[32];
   PT_CheckEvent     ptCheckEvent;
   
   
   // �����������ʱ��ѭ������
   while (g_DAQSystemData.usDAQThreadLoop)
   {
      // ���֪ͨ�¼�
      ptCheckEvent.EventType = &usEventType;
      ptCheckEvent.Milliseconds = 1000;

      ErrCde = DRV_CheckEvent (g_PCI1711.hDevice, &ptCheckEvent);
	  if (ErrCde != SUCCESS){
		  DRV_GetErrorMessage(ErrCde,lpszErrMsg);
		  ERR1("-AD�ɼ�����ѯ�¼�����DRV_CheckEvent������������룺%d���������򷵻ش���������%s",ErrCde,lpszErrMsg);
         continue;
	  }

      // �����������ı��¼�
      if (usEventType & ADS_EVT_BUFCHANGE)
         DAQFAIOnBufferChange ();

      // ��������¼�
      if (usEventType & ADS_EVT_OVERRUN)
         DAQFAIOnOverrun ();

      // ������ֹ�¼�
      if (usEventType & ADS_EVT_TERMINATED)
      {
         DAQFAIOnTerminate ();
         break;	   // �˳��ɼ��߳�
      }
   }
   return 0;
}

//------------------------------------------------------------------------------------------------
// �����������ݲɼ�
// ����:
//    ��
// ���:
//    ��
// ����:
//    0           �ɹ�
//    <0          ʧ��
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
   
   // �����жϲɼ���ʽ���ݱ��滺������ϵͳ����ʹ��
   ulBufferSize = g_PCI1711.ulBufferSize * sizeof (USHORT);
   if ((g_PCI1711.AcquisitionBuffer = (USHORT far *)GlobalAlloc (GPTR, ulBufferSize)) == 0)
   {
      lstrcpy ((char *)szErrMsg, "ȫ���ڴ治��,���ܹ������ڴ�!");
      MessageBox (NULL, (char *)szErrMsg, "����", MB_OK);
      return -1;
   }

   // ���俽��������
   ulBufferSize = g_PCI1711.ulBufferSize * sizeof (USHORT);
   if ((g_PCI1711.CopyBuffer = (USHORT far *)GlobalAlloc (GPTR, ulBufferSize)) == 0)
   {
      lstrcpy ((char *)szErrMsg, "ȫ���ڴ治��,���ܹ������ڴ�!");
      MessageBox (NULL, (char *)szErrMsg, "����", MB_OK);
      return -1;
   }

   // ���òɼ�����
   for (i = 0; i < 16; i ++)
      ausGainCode [i] = g_PCI1711.usGainCode;

   // ����ɼ���û�д򿪣����ش���
   if (g_PCI1711.hDevice == 0L)
      return -1;

   // �����¼����
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
   
   // ���������ж�ɨ��
   ptFAIIntScanStart.TrigSrc        = 0;  // �����ô���Դ��0���ڲ���1���ⲿ����
   ptFAIIntScanStart.cyclic         = 1;  // ���Ƿ�ѭ����0����ѭ����1��ѭ����
   ptFAIIntScanStart.StartChan      = g_PCI1711.usStart;
   ptFAIIntScanStart.NumChans       = g_PCI1711.usNumber;
   ptFAIIntScanStart.SampleRate     = g_PCI1711.ulBufferSize;  // 16K/s
   ptFAIIntScanStart.GainList       = &ausGainCode[ 0 ];
   ptFAIIntScanStart.count          = g_PCI1711.ulBufferSize;  // 16K
   ptFAIIntScanStart.IntrCount      = 2048;  // ��ʹ��FIFOʱ����ֵ������FIFO��С��һ�루FIFO_SIZE����
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

   // �����ػ��߳�����λ
   g_DAQSystemData.usDAQThreadLoop = TRUE;
   
   // �����ػ��߳�
   g_DAQSystemData.uhFAIThread = CreateThread (NULL, 0,
      (LPTHREAD_START_ROUTINE)&DAQFAIRoutine, NULL, 0, NULL);
   
   // ����ػ��߳�û�����������ش���
   if (g_DAQSystemData.uhFAIThread == NULL)
      return -1;
   
   // ��ȷ����
   return 0;
}

//------------------------------------------------------------------------------------------------
// ֹͣ�������ݲɼ�
// ����:
//    ��
// ���:
//    ��
// ����:
//    0           �ɹ�
//    <0          ʧ��
//------------------------------------------------------------------------------------------------
int __stdcall FAI_ScanStop(void)
{
   int   ErrCde;
   
   // Ҫ���ػ��߳̽���
   g_DAQSystemData.usDAQThreadLoop = FALSE;

   // ��ֹ�����ж�ɨ��
   if (g_PCI1711.hDevice != 0L)
   {
      ErrCde = DRV_FAITerminate (g_PCI1711.hDevice);
      if (ErrCde != SUCCESS)
      {
      }
   }

   // �ȴ��ػ��߳̽���
   WaitForSingleObject (g_DAQSystemData.uhFAIThread, 2000);
   CloseHandle (g_DAQSystemData.uhFAIThread);
   g_DAQSystemData.uhFAIThread = NULL;
      // �ͷŻ�����
   if (g_PCI1711.AcquisitionBuffer != NULL)
   {
      GlobalFree (g_PCI1711.AcquisitionBuffer);
      g_PCI1711.AcquisitionBuffer = NULL;
   }
   
   // �ͷŻ�����
   if (g_PCI1711.CopyBuffer != NULL)
   {
      GlobalFree (g_PCI1711.CopyBuffer);
      g_PCI1711.CopyBuffer = NULL;
   }
   return 0;
}

//------------------------------------------------------------------------------------------------