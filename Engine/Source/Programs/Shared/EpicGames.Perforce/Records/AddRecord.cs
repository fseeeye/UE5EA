// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace EpicGames.Perforce
{
	/// <summary>
	/// Contains information about an added file
	/// </summary>
	public class AddRecord
	{
		/// <summary>
		/// Path to the file in the depot
		/// </summary>
		[PerforceTag("depotFile")]
		public string DepotFile;

		/// <summary>
		/// Path to the file in the workspace
		/// </summary>
		[PerforceTag("clientFile")]
		public string ClientFile;

		/// <summary>
		/// The working revision number of the file that was synced
		/// </summary>
		[PerforceTag("workRev")]
		public int WorkingRevision;

		/// <summary>
		/// Action taken when syncing the file
		/// </summary>
		[PerforceTag("action")]
		public string Action;

		/// <summary>
		/// Type of the file
		/// </summary>
		[PerforceTag("type")]
		public string Type;

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		private AddRecord()
		{
			DepotFile = null!;
			ClientFile = null!;
			Action = null!;
			Type = null!;
		}

		/// <summary>
		/// Format this record for display in the debugger
		/// </summary>
		/// <returns>Summary of this revision</returns>
		public override string? ToString()
		{
			return DepotFile;
		}
	}
}
