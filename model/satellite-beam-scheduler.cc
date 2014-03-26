/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Magister Solutions Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Sami Rantanen <sami.rantanen@magister.fi>
 */

#include <algorithm>
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/singleton.h"
#include "satellite-rtn-link-time.h"
#include "satellite-beam-scheduler.h"

NS_LOG_COMPONENT_DEFINE ("SatBeamScheduler");

namespace ns3 {

bool SatBeamScheduler::CompareCno (const UtInfoItem_t &first, const UtInfoItem_t &second)
{
  double result = false;

  double cnoFirst = first.second->GetCnoEstimation ();
  double cnoSecond = second.second->GetCnoEstimation ();

  if ( !isnan (cnoFirst) )
    {
       if ( isnan (cnoSecond) )
         {
           result = false;
         }
       else
         {
           result = (cnoFirst < cnoSecond);
         }
    }

  return result;
}

// UtInfo class declarations for SatBeamScheduler
SatBeamScheduler::SatUtInfo::SatUtInfo ( Ptr<SatDamaEntry> damaEntry, Ptr<SatCnoEstimator> cnoEstimator )
 : m_damaEntry (damaEntry),
   m_cnoEstimator (cnoEstimator)
{
  NS_LOG_FUNCTION (this);
}

Ptr<SatDamaEntry>
SatBeamScheduler::SatUtInfo::GetDamaEntry ()
{
  NS_LOG_FUNCTION (this);

  return m_damaEntry;
}

void
SatBeamScheduler::SatUtInfo::UpdateDamaEntriesFromCrs ()
{
  NS_LOG_FUNCTION (this);

  for ( CrMsgContainer_t::const_iterator crIt = m_crContainer.begin (); crIt != m_crContainer.end (); crIt++ )
    {
      SatCrMessage::RequestContainer_t crContent = (*crIt)->GetCapacityRequestContent ();

      for ( SatCrMessage::RequestContainer_t::const_iterator descriptorIt = crContent.begin (); descriptorIt != crContent.end (); descriptorIt++ )
        {
          switch (descriptorIt->first.second)
          {
            case SatEnums::DA_RBDC:
              m_damaEntry->UpdateRbdcInKbps (descriptorIt->first.first, descriptorIt->second);
              break;

            case SatEnums::DA_VBDC:
              m_damaEntry->UpdateVbdcInBytes (descriptorIt->first.first, descriptorIt->second);
              break;

            case SatEnums::DA_AVBDC:
              m_damaEntry->SetVbdcInBytes (descriptorIt->first.first, descriptorIt->second);
              break;

            default:
              break;
          }
        }
    }
        // clear container when CRs processed
    m_crContainer.clear ();
}

double
SatBeamScheduler::SatUtInfo::GetCnoEstimation ()
{
  NS_LOG_FUNCTION (this);

  return m_cnoEstimator->GetCnoEstimation ();
}

void
SatBeamScheduler::SatUtInfo::AddCnoSample (double sample)
{
  NS_LOG_FUNCTION (this << sample);

  m_cnoEstimator->AddSample (sample);
}

void
SatBeamScheduler::SatUtInfo::AddCrMsg (Ptr<SatCrMessage> crMsg)
{
  NS_LOG_FUNCTION (crMsg);

  m_crContainer.push_back (crMsg);
}

// SatBeamScheduler

NS_OBJECT_ENSURE_REGISTERED (SatBeamScheduler);

TypeId 
SatBeamScheduler::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SatBeamScheduler")
    .SetParent<Object> ()
    .AddConstructor<SatBeamScheduler> ()
    .AddAttribute ("CnoEstimationMode",
                   "Mode of the C/N0 estimator",
                   EnumValue (SatCnoEstimator::LAST),
                   MakeEnumAccessor (&SatBeamScheduler::m_cnoEstimatorMode),
                   MakeEnumChecker (SatCnoEstimator::LAST, "Last value in window used.",
                                    SatCnoEstimator::MINIMUM, "Minimum value in window used.",
                                    SatCnoEstimator::AVERAGE, "Average value in window used."))
    .AddAttribute( "CnoEstimationWindow",
                   "Time window for C/N0 estimation.",
                   TimeValue (MilliSeconds (1000)),
                   MakeTimeAccessor (&SatBeamScheduler::m_cnoEstimationWindow),
                   MakeTimeChecker ())
    .AddAttribute( "MaxTwoWayPropagationDelay",
                   "Maximum two way propagation delay between GW and UT.",
                   TimeValue (MilliSeconds (560)),
                   MakeTimeAccessor (&SatBeamScheduler::m_maxTwoWayPropagationDelay),
                   MakeTimeChecker ())
    .AddAttribute( "MaxTBTPTxAndProcessingDelay",
                   "Maximum TBTP transmission and processing delay at the GW.",
                   TimeValue (MilliSeconds (100)),
                   MakeTimeAccessor (&SatBeamScheduler::m_maxTBTPTxAndProcessingDelay),
                   MakeTimeChecker ())
  ;
  return tid;
}

SatBeamScheduler::SatBeamScheduler ()
  : m_beamId (0),
    m_superframeSeq (0),
    m_superFrameCounter (0),
    m_txCallback (0),
    m_tbtpAddCb (0),
    m_currentFrame (0),
    m_totalSlotsLeft (0),
    m_additionalSlots (0),
    m_slotsPerUt (0),
    m_craBasedBytes (0),
    m_rbdcBasedBytes (0),
    m_vbdcBasedBytes (0),
    m_cnoEstimatorMode (SatCnoEstimator::LAST),
    m_maxBbFrameSize (0)
{
  NS_LOG_FUNCTION (this);

  m_currentUt = m_utSortedInfos.end ();
  m_currentCarrier = m_carrierIds.end ();
  m_currentSlot = m_timeSlots.end ();
}

SatBeamScheduler::~SatBeamScheduler ()
{
  NS_LOG_FUNCTION (this);
}

void
SatBeamScheduler::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_txCallback.Nullify ();
  Object::DoDispose ();
}

void
SatBeamScheduler::Receive (Ptr<Packet> packet)
{
  NS_LOG_FUNCTION (this << packet);
}

bool
SatBeamScheduler::Send (Ptr<SatControlMessage> msg)
{
  NS_LOG_FUNCTION (this << msg);
  NS_LOG_LOGIC ("p=" << msg );

  m_txCallback (msg, Mac48Address::GetBroadcast ());

  return true;
}

void
SatBeamScheduler::Initialize (uint32_t beamId, SatBeamScheduler::SendCtrlMsgCallback cb, Ptr<SatSuperframeSeq> seq, uint8_t maxRcCount, uint32_t maxFrameSizeInBytes)
{
  NS_LOG_FUNCTION (this << beamId << &cb);

  m_beamId = beamId;
  m_txCallback = cb;
  m_superframeSeq = seq;
  m_maxBbFrameSize = maxFrameSizeInBytes;

  /**
   * Calculating to start time for superframe counts to start the scheduling from.
   * The offset is calculated by estimating the maximum delay between GW and UT,
   * so that the sent TBTP will be received by UT in time to be able to still send
   * the packet in time.
   */
  Time totalDelay = m_maxTwoWayPropagationDelay + m_maxTBTPTxAndProcessingDelay;
  uint32_t sfCountOffset = (uint32_t)(totalDelay.GetInteger () / seq->GetDuration (0).GetInteger () + 1);

  // Scheduling starts after one empty super frame.
  m_superFrameCounter = Singleton<SatRtnLinkTime>::Get ()->GetNextSuperFrameCount (m_currentSequence) + sfCountOffset;

  // TODO: If RA channel is wanted to allocate to UT with some other means than randomizing
  // this part of implementation is needed to change
  m_raChRandomIndex = CreateObject<UniformRandomVariable> ();
  m_raChRandomIndex->SetAttribute ("Min", DoubleValue (0));

  // by default we give index 0, even if there is no RA channels configured.
  uint32_t maxIndex = 0;

  if ( m_superframeSeq->GetSuperframeConf (m_currentSequence)->GetRaChannelCount () > 0 )
    {
      maxIndex = m_superframeSeq->GetSuperframeConf (m_currentSequence)->GetRaChannelCount () - 1;
    }

  m_raChRandomIndex->SetAttribute("Max", DoubleValue (maxIndex));
  m_frameAllocator = CreateObject<SatFrameAllocator> (m_superframeSeq->GetSuperframeConf (m_currentSequence), m_superframeSeq->GetWaveformConf (), maxRcCount);

  NS_LOG_LOGIC ("Initialize SatBeamScheduler at " << Simulator::Now ().GetSeconds ());

  Time delay;
  Time txTime = Singleton<SatRtnLinkTime>::Get ()->GetNextSuperFrameStartTime (m_currentSequence);

  if (txTime > Now ())
    {
      delay = txTime - Now ();
    }
  else
    {
      NS_FATAL_ERROR ("Trying to schedule a superframe in the past!");
    }

  Simulator::Schedule (delay, &SatBeamScheduler::Schedule, this);
}

uint32_t
SatBeamScheduler::AddUt (Address utId, Ptr<SatLowerLayerServiceConf> llsConf)
{
  NS_LOG_FUNCTION (this << utId);

  Ptr<SatDamaEntry> damaEntry = Create<SatDamaEntry> (llsConf);
  Ptr<SatCnoEstimator> cnoEstimator = CreateCnoEstimator ();

  Ptr<SatUtInfo> utInfo = Create<SatUtInfo> (damaEntry, cnoEstimator);

  // TODO: CAC check needed to add

  std::pair<UtInfoMap_t::iterator, bool > result = m_utInfos.insert (std::make_pair (utId, utInfo));

  if (result.second)
    {
      m_utSortedInfos.push_back (std::make_pair (utId, utInfo));
    }
  else
    {
      NS_FATAL_ERROR ("UT (Address: " << utId << ") already added to Beam scheduler.");
    }

  m_superframeSeq->GetSuperframeConf (m_currentSequence)->GetRaChannelCount ();

  // return random RA channel index for the UT.
  return m_raChRandomIndex->GetInteger ();
}

void
SatBeamScheduler::UpdateUtCno (Address utId, double cno)
{
  NS_LOG_FUNCTION (this << utId << cno);

  // check that UT is added to this scheduler.
  UtInfoMap_t::iterator result = m_utInfos.find (utId);
  NS_ASSERT (result != m_utInfos.end ());

  m_utInfos[utId]->AddCnoSample (cno);
}

void
SatBeamScheduler::UtCrReceived (Address utId, Ptr<SatCrMessage> crMsg)
{
  NS_LOG_FUNCTION (this << utId << crMsg);

  // check that UT is added to this scheduler.
  UtInfoMap_t::iterator result = m_utInfos.find (utId);
  NS_ASSERT (result != m_utInfos.end ());

  m_utInfos[utId]->AddCrMsg (crMsg);
}

Ptr<SatCnoEstimator>
SatBeamScheduler::CreateCnoEstimator ()
{
  NS_LOG_FUNCTION (this);

  Ptr<SatCnoEstimator> estimator = NULL;

  switch (m_cnoEstimatorMode)
  {
    case SatCnoEstimator::LAST:
    case SatCnoEstimator::MINIMUM:
    case SatCnoEstimator::AVERAGE:
      estimator = Create<SatBasicCnoEstimator> (m_cnoEstimatorMode, m_cnoEstimationWindow);
      break;

    default:
      NS_FATAL_ERROR ("Not supported C/N0 estimation mode!!!");
      break;

  }

  return estimator;
}

void
SatBeamScheduler::Schedule ()
{
  NS_LOG_FUNCTION (this);

  // check that there is UTs to schedule
  if ( m_utInfos.size() > 0 )
    {
      UpdateDamaEntries ();

      DoPreResourceAllocation ();


      Ptr<SatTbtpMessage> firstTbtp = CreateObject<SatTbtpMessage> (m_currentSequence);
      firstTbtp->SetSuperframeCounter (m_superFrameCounter++);

      std::vector<Ptr<SatTbtpMessage> > tbtps;
      tbtps.push_back (firstTbtp);

      // Add RA slots (channels)
      AddRaChannels (tbtps);

      // add DA slots to TBTP(s)
      m_frameAllocator->GenerateTimeSlots (tbtps, m_maxBbFrameSize);

      // send TBTPs
      for ( std::vector <Ptr<SatTbtpMessage> > ::const_iterator it = tbtps.begin (); it != tbtps.end (); it++ )
        {
          Send (*it);
        }

      NS_LOG_LOGIC ("TBTP sent at: " << Simulator::Now ().GetSeconds ());
    }

  // re-schedule next TBTP sending (call of this function)
  Simulator::Schedule ( m_superframeSeq->GetDuration (m_currentSequence), &SatBeamScheduler::Schedule, this);
}

void
SatBeamScheduler::AddRaChannels (std::vector <Ptr<SatTbtpMessage> >& tbtpContainer)
{
  NS_LOG_FUNCTION (this);

  if (tbtpContainer.empty ())
    {
      NS_FATAL_ERROR ("TBTP container must contain at least one message.");
    }

  Ptr<SatTbtpMessage> tbtpToFill = tbtpContainer.back ();

  Ptr<SatSuperframeConf> superFrameConf = m_superframeSeq->GetSuperframeConf (m_currentSequence);

  for (uint32_t i = 0; i < superFrameConf->GetRaChannelCount (); i++)
    {
      uint8_t frameId = superFrameConf->GetRaChannelFrameId (i);
      Ptr<SatFrameConf> frameConf = superFrameConf->GetFrameConf (frameId);
      uint16_t timeSlotCount = frameConf->GetTimeSlotCount() / frameConf->GetCarrierCount();

      if ( timeSlotCount > 0 )
        {
          if ( (tbtpToFill->GetSizeInBytes () + (tbtpToFill->GetTimeSlotInfoSizeInBytes () * timeSlotCount)) > m_maxBbFrameSize )
            {
              Ptr<SatTbtpMessage> newTbtp = CreateObject<SatTbtpMessage> (tbtpToFill->GetSuperframeSeqId ());
              newTbtp->SetSuperframeCounter ( tbtpToFill->GetSuperframeCounter ());

              tbtpContainer.push_back (newTbtp);

              tbtpToFill = newTbtp;
            }

          tbtpToFill->SetRaChannel (i, superFrameConf->GetRaChannelFrameId (i), timeSlotCount);
        }
    }
}

void
SatBeamScheduler::UpdateDamaEntries ()
{
  NS_LOG_FUNCTION (this);

  // reset requested bytes per SF for each (RC_index, CC)
  m_craBasedBytes = 0;
  m_rbdcBasedBytes = 0;
  m_vbdcBasedBytes = 0;

  for (UtInfoMap_t::iterator it = m_utInfos.begin (); it != m_utInfos.end (); it ++ )
    {
      // estimation of the C/N0 is done when scheduling UT

      Ptr<SatDamaEntry> damaEntry = it->second->GetDamaEntry ();

      // process received CRs
      it->second->UpdateDamaEntriesFromCrs ();

      // calculate (update) requested bytes per SF for each (RC_index, CC)
      m_craBasedBytes += damaEntry->GetCraBasedBytes (m_superframeSeq->GetDuration (m_currentSequence).GetSeconds ());
      m_rbdcBasedBytes += damaEntry->GetRbdcBasedBytes (m_superframeSeq->GetDuration (m_currentSequence).GetSeconds ());
      m_vbdcBasedBytes += damaEntry->GetVbdcBasedBytes ();
      
      // decrease persistence values
      damaEntry->DecrementDynamicRatePersistence ();
      damaEntry->DecrementVolumeBacklogPersistence ();
    }
}

void SatBeamScheduler::DoPreResourceAllocation ()
{
  NS_LOG_FUNCTION (this);

  m_totalSlotsLeft = 0;

  if ( m_utInfos.size () > 0 )
    {
      // sort UTs according to C/N0
      m_utSortedInfos.sort (CompareCno);

      m_currentUt = m_utSortedInfos.begin ();

      m_frameAllocator->RemoveAllocations ();

      for (UtSortedInfoContainer_t::iterator it = m_utSortedInfos.begin (); it != m_utSortedInfos.end (); it++)
        {
          Ptr<SatDamaEntry> damaEntry = it->second->GetDamaEntry ();
          SatFrameAllocator::SatFrameAllocReqItemContainer_t allocReqContainer;

          for (uint8_t i = 0; i < damaEntry->GetRcCount (); i++ )
            {
              SatFrameAllocator::SatFrameAllocReqItem rcAllocReq;

              rcAllocReq.m_craInKbps = damaEntry->GetCraInKbps (i);
              rcAllocReq.m_minRbdcInKbps = damaEntry->GetMinRbdcInKbps (i);
              rcAllocReq.m_rbdcInKbps = damaEntry->GetRbdcInKbps (i);
              rcAllocReq.m_vbdcBytes = damaEntry->GetVbdcInBytes (i);

              allocReqContainer.push_back (rcAllocReq);
            }

          SatFrameAllocator::SatFrameAllocReq allocReq (allocReqContainer);
          allocReq.m_address = it->first;

          SatFrameAllocator::SatFrameAllocResp allocResp;

          m_frameAllocator->AllocateToFrame (it->second->GetCnoEstimation (), allocReq, allocResp);
        }

      m_frameAllocator->AllocateSymbols ();
    }
}

} // namespace ns3
