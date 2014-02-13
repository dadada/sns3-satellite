/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Magister Solutions Ltd.
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
 * Author: Frans Laakso <frans.laakso@magister.fi>
 */
#include "satellite-random-access-allocation-channel.h"

NS_LOG_COMPONENT_DEFINE ("SatRandomAccessAllocationChannel");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (SatRandomAccessAllocationChannel);

TypeId 
SatRandomAccessAllocationChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SatRandomAccessAllocationChannel")
    .SetParent<Object> ()
    ;
  return tid;
}

SatRandomAccessAllocationChannel::SatRandomAccessAllocationChannel () :
  m_crdsaMinRandomizationValue (),
  m_crdsaMaxRandomizationValue (),
  m_crdsaNumOfInstances (),
  m_crdsaMinIdleBlocks (),
  m_crdsaIdleBlocksLeft (0),
  m_crdsaBackoffTime (),
  m_crdsaBackoffProbability (),
  m_crdsaMaximumBackoffProbability (),
  m_crdsaMaxUniquePayloadPerBlock (),
  m_crdsaMaxConsecutiveBlocksAccessed (),
  m_crdsaNumOfConsecutiveBlocksUsed (0),
  m_crdsaBackoffReleaseTime (0)
{
  NS_LOG_FUNCTION (this);

  /// TODO add here init from lower layer service conf

}

SatRandomAccessAllocationChannel::~SatRandomAccessAllocationChannel ()
{
  NS_LOG_FUNCTION (this);
}

void
SatRandomAccessAllocationChannel::DoCrdsaVariableSanityCheck ()
{
  NS_LOG_FUNCTION (this);

  if (m_crdsaMinRandomizationValue < 0 || m_crdsaMaxRandomizationValue < 0)
    {
      NS_FATAL_ERROR ("SatRandomAccessAllocationChannel::DoCrdsaVariableSanityCheck - min < 0 || max < 0");
    }

  if (m_crdsaMinRandomizationValue > m_crdsaMaxRandomizationValue)
    {
      NS_FATAL_ERROR ("SatRandomAccessAllocationChannel::DoCrdsaVariableSanityCheck - min > max");
    }

  if (m_crdsaNumOfInstances < 1)
    {
      NS_FATAL_ERROR ("SatRandomAccessAllocationChannel::DoCrdsaVariableSanityCheck - instances < 1");
    }

  if ( (m_crdsaMaxRandomizationValue - m_crdsaMinRandomizationValue) < m_crdsaNumOfInstances)
    {
      NS_FATAL_ERROR ("SatRandomAccessAllocationChannel::DoCrdsaVariableSanityCheck - (max - min) < instances");
    }

  if (m_crdsaBackoffTime < 0)
    {
      NS_FATAL_ERROR ("SatRandomAccessAllocationChannel::CrdsaDoVariableSanityCheck - m_crdsaBackoffTime < 0");
    }

  if (m_crdsaBackoffProbability < 0.0 || m_crdsaBackoffProbability > 1.0)
    {
      NS_FATAL_ERROR ("SatRandomAccessAllocationChannel::CrdsaDoVariableSanityCheck - m_crdsaBackoffProbability < 0.0 || m_crdsaBackoffProbability > 1.0");
    }

  if (m_crdsaMaximumBackoffProbability < 0.0 || m_crdsaMaximumBackoffProbability > 1.0)
    {
      NS_FATAL_ERROR ("SatRandomAccessAllocationChannel::CrdsaDoVariableSanityCheck - m_crdsaMaximumBackoffProbability < 0.0 || m_crdsaMaximumBackoffProbability > 1.0");
    }

  NS_LOG_INFO ("SatRandomAccessAllocationChannel::DoCrdsaVariableSanityCheck - Variable sanity check done");
}

} // namespace ns3
