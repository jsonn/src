static int cpumatch(struct device *, struct cfdata *, void *);
static void cpuattach(struct device *, struct device *, void *);

CFATTACH_DECL(cpu, sizeof(struct device),
    cpumatch, cpuattach, NULL, NULL);

extern struct cfdriver cpu_cd;

int
cpumatch(struct device *parent, struct cfdata *cf, void *aux)
{
	struct mainbus_attach_args *mba = aux;

	if (strcmp(mba->mba_name, cpu_cd.cd_name) != 0)
		return 0;

	if (cpu_info_store.ci_dev != NULL)
		return 0;

	return 1;
}

void
cpuattach(struct device *parent, struct device *self, void *aux)
{
	(void) cpu_attach_common(self, 0);
}
