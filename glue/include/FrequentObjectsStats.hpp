/*******************************************************************************
 * Copyright (c) 2010, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(FREQUENTOBJECTSSTATS_HPP_)
#define FREQUENTOBJECTSSTATS_HPP_

#include "objectdescription.h"
#include "spacesaving.h"

#include "Base.hpp"

class MM_EnvironmentBase;

#define TOPK_FREQUENT_DEFAULT 10
#define K_TO_SIZE_RATIO 8

/*
 * Keeps track of the most frequent class instantiations
 */

class MM_FrequentObjectsStats : public MM_Base
{
private:

	/*
	 * Estimates the space necessary to report the top k elements accurately 90% of the time.
	 * Derived from a sample run of Eclipse, which showed that the size necessary to have accurately report the top k
	 * elements was approximately linear.
	 */
	uint32_t
	getSizeForTopKFrequent(uint32_t topKFrequent)
	{
		/* DO NOTHING */
		return 0;
	}

/* Function Members */
public:
	static MM_FrequentObjectsStats *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	/* reset the stats*/
	void clear()
	{
		/* DO NOTHING */
		return;
	}

	/*
	 * Update stats with another class
	 * @param clazz another clazz to record
	 */
	void
	update(MM_EnvironmentBase *env, omrobjectptr_t object)
	{
		/* DO NOTHING */
		return;
	}

	/* Creates a data structure which keeps track of the k most frequent class allocations (estimated probability of 90% of
	 * reporting this accurately (and in the correct order).  The larger k is, the more memory is required
	 * @param portLibrary the port library
	 * @param k the number of frequent objects we'd like to accurately report
	 */
	MM_FrequentObjectsStats(OMRPortLibrary *portLibrary, uint32_t k=TOPK_FREQUENT_DEFAULT)
	{}


	/* Merges a FrequentObjectStats structures together together with this one*/
	void merge(MM_FrequentObjectsStats* frequentObjectsStats);

	void traceStats(MM_EnvironmentBase *env);

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
};

#endif /* !FREQUENTOBJECTSSTATS_HPP_ */
